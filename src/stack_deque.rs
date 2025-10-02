use std::{ops::{Index, IndexMut}, ptr};


#[derive(Debug, Clone, PartialEq, Eq)]
pub struct StackDeque<T: Copy + 'static>
{
	buffer: Vec<T>,
	pub len: usize,
	capacity: usize,
	pos: usize,
}

impl<T: Copy + 'static> StackDeque<T>
{
	pub fn new(capacity: usize) -> Self
	{
		let mut buffer = Vec::with_capacity(3 * capacity);
		unsafe { buffer.set_len(3 * capacity); }

		Self {
			buffer,
			len: 0,
			capacity,
			pos: capacity,
		}
	}

	pub fn push_back(&mut self, value: T)
	{
		assert!(self.len != self.capacity);
		if self.pos == self.capacity * 2
		{
			unsafe {
				ptr::copy(self.buffer.as_ptr().add(self.pos), self.buffer.as_mut_ptr().add(self.capacity), self.len);
			}
			self.pos = self.capacity;
		}
		self.buffer[self.pos + self.len] = value;
		self.len += 1;
	}

	pub fn pop_back(&mut self)
	{
		assert!(self.len != 0);
		self.len -= 1;
	}

	pub fn push_front(&mut self, value: T)
	{
		assert!(self.len != self.capacity);
		if self.pos == 0
		{
			unsafe {
				ptr::copy(self.buffer.as_ptr().add(self.pos), self.buffer.as_mut_ptr().add(self.capacity), self.len);
			}
			self.pos = self.capacity;
		}
		self.buffer[self.pos - 1] = value;
		self.pos -= 1;
		self.len += 1;
	}

	pub fn pop_front(&mut self)
	{
		assert!(self.len != 0);
		self.pos += 1;
	}

	pub fn len(&self) -> usize
	{
		self.len
	}

    pub fn iter(&self) -> StackDequeIter<'_, T> {
        StackDequeIter {
            slice: &self.buffer[self.pos..self.pos + self.len],
            index: 0,
        }
    }

    pub fn iter_mut(&mut self) -> StackDequeIterMut<'_, T> {
        let slice = &mut self.buffer[self.pos..self.pos + self.len];
        StackDequeIterMut { slice, index: 0 }
    }
}

impl<T: Copy + 'static> Index<usize> for StackDeque<T> {
    type Output = T;

    fn index(&self, index: usize) -> &Self::Output {
		assert!(index < self.len());
        &self.buffer[self.pos + index]
    }
}

impl<T: Copy + 'static> IndexMut<usize> for StackDeque<T> {

    fn index_mut(&mut self, index: usize) -> &mut Self::Output {
		assert!(index < self.len());
        &mut self.buffer[self.pos + index]
    }
}

pub struct StackDequeIter<'a, T: Copy + 'static>
{
	slice: &'a [T],
	index: usize,
}

impl<'a, T: Copy + 'static> Iterator for StackDequeIter<'a, T>
{
    type Item = &'a T;

    fn next(&mut self) -> Option<Self::Item> {
		if self.index > self.slice.len()
		{
			None
		}
		else
		{
			let v = &self.slice[self.index];
			self.index += 1;
			Some(v)
		}
    }

	fn size_hint(&self) -> (usize, Option<usize>) {
	    let remaining = self.slice.len() - self.index;
		(remaining, Some(remaining))
	}
}

pub struct StackDequeIterMut<'a, T: Copy + 'static>
{
	slice: &'a mut [T],
	index: usize,
}

impl<'a, T: Copy + 'static> Iterator for StackDequeIterMut<'a, T>
{
    type Item = &'a mut T;

    fn next(&mut self) -> Option<Self::Item> {
		if self.index > self.slice.len()
		{
			None
		}
		else
		{
			let v = std::mem::take(&mut self.slice);
			let (head, tail) = v.split_at_mut(1);
			self.slice = tail;
			self.index += 1;
			Some(&mut head[0])
		}
    }

	fn size_hint(&self) -> (usize, Option<usize>) {
	    let remaining = self.slice.len() - self.index;
		(remaining, Some(remaining))
	}
}
