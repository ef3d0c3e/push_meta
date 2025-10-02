use rand::seq::SliceRandom;
use rand::SeedableRng;
use rand_chacha::ChaCha8Rng;

use strum::IntoEnumIterator;
use strum_macros::EnumIter;
use crate::ops::Op;
use crate::sort_small::Block;
use crate::stack_deque::StackDeque;
use crate::sort_small::sort_small;
mod ops;
mod sort_small;
mod stack_deque;

// implement access operator for StackDeque[usize] ...

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SaveState {
	pub stack_a: StackDeque<i32>,
	pub stack_b: StackDeque<i32>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct State {
	pub stack_a: StackDeque<i32>,
	pub stack_b: StackDeque<i32>,
	pub ops: Vec<Op>,
	pub saves: Vec<SaveState>,
}

impl State {
	fn new(values: &[i32]) -> Self {
		let mut mapped = values
			.iter()
			.enumerate()
			.map(|(idx, val)| (idx, *val))
			.collect::<Vec<(usize, i32)>>();
		mapped.sort_by_key(|(_, val)| *val);
		let mut stack_a = StackDeque::new(values.len());
		stack_a.len = values.len();
		for (val, (idx, _)) in mapped.drain(..).enumerate() {
			stack_a[idx] = val as i32;
		}

		Self {
			stack_a,
			stack_b: StackDeque::new(values.len()),
			ops: Vec::default(),
			saves: Vec::default(),
		}
	}

	fn from_save(&self, save: usize) -> Self {
		assert!(save < self.saves.len());
		let st = self.saves[save].clone();
		
		Self {
			stack_a: st.stack_a,
			stack_b: st.stack_b,
			ops: self.ops[0..save].iter().copied().collect::<Vec<_>>(),
			saves: vec![],
		}
	}

	fn print(&self, ops: bool) {
		println!(" A\n---");
		for x in self.stack_a.iter() {
			println!("{x}")
		}
		println!(" B\n---");
		for x in self.stack_b.iter() {
			println!("{x}")
		}
		if !ops {
			return;
		}
		println!("Ops\n---");
		for op in &self.ops {
			println!("{op}")
		}
	}

	fn op_unsaved(&mut self, op: Op) {
		match op {
			Op::Pa => {
				if self.stack_b.len() != 0 {
					let v = *self.stack_b.front();
					self.stack_b.pop_front();
					self.stack_a.push_front(v);
				}
			}
			Op::Pb => {
				if self.stack_a.len() != 0 {
					let v = *self.stack_a.front();
					self.stack_a.pop_front();
					self.stack_b.push_front(v);
				}
			}
			Op::Sa => {
				if self.stack_a.len() >= 2 {
					let first = self.stack_a[0];
					let second = self.stack_a[1];
					self.stack_a[0] = second;
					self.stack_a[1] = first;
				}
			}
			Op::Sb => {
				if self.stack_b.len() >= 2 {
					let first = self.stack_b[0];
					let second = self.stack_b[1];
					self.stack_b[0] = second;
					self.stack_b[1] = first;
				}
			}
			Op::Ss => {
				if self.stack_a.len() >= 2 {
					let first = self.stack_a[0];
					let second = self.stack_a[1];
					self.stack_a[0] = second;
					self.stack_a[1] = first;
				}
				if self.stack_b.len() >= 2 {
					let first = self.stack_b[0];
					let second = self.stack_b[1];
					self.stack_b[0] = second;
					self.stack_b[1] = first;
				}
			}
			Op::Ra => {
				if self.stack_a.len() != 0 {
					let v = *self.stack_a.front();
					self.stack_a.pop_front();
					self.stack_a.push_back(v);
				}
			}
			Op::Rb => {
				if self.stack_b.len() != 0 {
					let v = *self.stack_b.front();
					self.stack_b.pop_front();
					self.stack_b.push_back(v);
				}
			}
			Op::Rr => {
				if self.stack_a.len() != 0 {
					let v = *self.stack_a.front();
					self.stack_a.pop_front();
					self.stack_a.push_back(v);
				}
				if self.stack_b.len() != 0 {
					let v = *self.stack_b.front();
					self.stack_b.pop_front();
					self.stack_b.push_back(v);
				}
			}
			Op::Rra => {
				if self.stack_a.len() != 0 {
					let v = *self.stack_a.back();
					self.stack_a.pop_back();
					self.stack_a.push_front(v);
				}
			}
			Op::Rrb => {
				if self.stack_b.len() != 0 {
					let v = *self.stack_b.back();
					self.stack_b.pop_back();
					self.stack_b.push_front(v);
				}
			}
			Op::Rrr => {
				if self.stack_a.len() != 0 {
					let v = *self.stack_a.back();
					self.stack_a.pop_back();
					self.stack_a.push_front(v);
				}
				if self.stack_b.len() != 0 {
					let v = *self.stack_b.back();
					self.stack_b.pop_back();
					self.stack_b.push_front(v);
				}
			}
		}
		self.ops.push(op);
	}

	fn op(&mut self, op: Op) {
		self.saves.push(SaveState {
			stack_a: self.stack_a.clone(),
			stack_b: self.stack_b.clone(),
		});

		self.op_unsaved(op);
	}

	fn ops(&mut self, ops: &[Op]) {
		for op in ops
		{
			self.op(*op);
		}
	}

	fn choose_pivot(stack: &StackDeque<i32>, len: usize) -> i32
	{
		assert!(stack.len() >= len);
		let mut v: Vec<i32> = stack.iter().take(len).copied().collect();
		v.sort_unstable();
		v[len / 2]
	}

	fn sort_stack_b(&mut self, n: usize) {
		if n == 0 { return; }
		assert!(self.stack_b.len() >= n);

        if n <= 3 {
			self.ops(sort_small(self, Block::BlkBTop, n));
            return;
        }

		// general partitioning on B: move >= pivot to A, rotate others
		let pivot = Self::choose_pivot(&self.stack_b, n);
		let mut pushed = 0usize; // number moved to A
		let mut rotated = 0usize;

		for _ in 0..n {
			if *self.stack_b.front() >= pivot {
				self.op(Op::Pa);
				pushed += 1;
			} else {
				self.op(Op::Rb);
				rotated += 1;
			}
		}

		// undo rotations on B
		for _ in 0..rotated {
			self.op(Op::Rrb);
		}

		// now recursively sort the chunk we moved to A, then continue with the rest of B
		self.sort_stack_a(pushed);
		self.sort_stack_b(n - pushed);
	}

	fn sort_stack_a(&mut self, n: usize) {
        if n <= 1 { return; }
        assert!(self.stack_a.len() >= n);

        if n <= 3 {
			self.ops(sort_small(self, Block::BlkATop, n));
            return;
        }

        let pivot = Self::choose_pivot(&self.stack_a, n);
        let mut pushed = 0usize;
        let mut rotated = 0usize;

        // partition: push <= pivot to B, rotate otherwise
        for _ in 0..n {
            if *self.stack_a.front() <= pivot {
                self.op(Op::Pb);
                pushed += 1;
            } else {
                self.op(Op::Ra);
                rotated += 1;
            }
        }

        // undo the rotations so the remaining (n - pushed) elements in A are back in order
        for _ in 0..rotated {
            self.op(Op::Rra);
        }

        // recursively sort the chunk that stayed in A, then sort the chunk we pushed to B
        self.sort_stack_a(n - pushed);
        self.sort_stack_b(pushed);
	}

	fn find_state(&self, st: &SaveState, start: usize, len: usize) -> Option<usize>
	{
		let end = (start + len).min(self.saves.len());
		let mut best = None;
		for i in start..end
		{
			if &self.saves[i] == st
			{
				best = Some(i)
			}
		}
		best
	}
}

#[derive(Debug)]
pub struct CompressorSkip {
	pub ops: Vec<Op>,
	pub skip: usize,
}

#[derive(Debug)]
pub struct Compressor<'s>
{
	pub max_depth: usize,
	pub max_len: usize,

	pub skips: Vec<CompressorSkip>,

	pub skip: usize,
	pub ops: Vec<Op>,

	pub state: &'s State,
}

#[derive(Debug, Clone)]
pub struct CompressorState
{
	pub ops: Vec<Op>,
	pub depth: usize,
	pub skip: usize,
}

impl<'s> Compressor<'s> {
	pub fn new(state: &'s State, max_depth: usize, max_len: usize) -> Self {
		Self {
			max_depth,
			max_len,
			skips: vec![],
			skip: 0,
			ops: vec![],
			state,
		}
	}

	pub fn compress(&mut self, mut comp: CompressorState, index: usize) -> (usize, Vec<Op>)
	{
		let mut best_ops = None;
		let mut best_skip = None;
		let mut skip = 0;
		for op in Op::iter()
		{
			let mut cloned = self.state.from_save(index);
			comp.ops.push(op);
			cloned.op(op);

			if let Some(fut) = self.state.find_state(&cloned.saves[0], index + 1, self.max_len) {
				skip = fut - index - comp.depth;
				if skip >= best_skip.unwrap_or(0) {
					best_ops = Some(vec![op]);
					best_skip = Some(skip);
				}
			}
			if comp.depth < self.max_depth
			{
				let mut new = comp.clone();
				new.depth += 1;
				let (skip, ops) = self.compress(new, index);
				if skip >= best_skip.unwrap_or(0) {
					best_ops = Some(ops);
					best_skip = Some(skip);
				}
			}
			else
			{
				// Find best
			}

			comp.ops.pop();
			self.ops.pop();
		}
		
		(best_skip.unwrap_or(0), best_ops.unwrap_or(vec![]))
	}
}

fn main() {
	let data = [0u8; 32];
	let mut seed: <ChaCha8Rng as SeedableRng>::Seed = data;
	let mut rng = ChaCha8Rng::from_seed(seed);

	let mut numbers = (0..500).collect::<Vec<i32>>();
	numbers.shuffle(&mut rng);

	let mut s = State::new(numbers.as_slice());
	s.sort_stack_a(s.stack_a.len());
	s.print(true);
	println!("sorted: {} in {} ops", s.stack_a.iter().is_sorted() && s.stack_b.is_empty(), s.ops.len());
	s = s.compress(2, 500);
	println!("sorted: {} in {} ops", s.stack_a.iter().is_sorted() && s.stack_b.is_empty(), s.ops.len());
}
