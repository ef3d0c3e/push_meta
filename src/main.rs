use std::collections::vec_deque;
use std::collections::VecDeque;
use std::fmt::Display;
use std::ops::Index;
use std::ops::IndexMut;
use std::ptr;

use rand::seq::SliceRandom;
use rand::thread_rng;
use rand::Rng;
use rand::SeedableRng;
use rand_chacha::ChaCha8Rng;

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
}
