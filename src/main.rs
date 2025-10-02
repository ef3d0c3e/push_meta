use std::{collections::{vec_deque, VecDeque}, fmt::Display, ops::{Index, IndexMut}, ptr};

use crate::{ops::Op, stack_deque::StackDeque};
mod stack_deque;
mod ops;

// implement access operator for StackDeque[usize] ...

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SaveState
{
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
	fn new(values: &[i32]) -> Self
	{
		let mut mapped = values.iter()
			.enumerate()
			.map(|(idx, val)| (idx, *val))
			.collect::<Vec<(usize, i32)>>();
		mapped.sort_by_key(|(_, val)| *val);
		let mut stack_a = StackDeque::new(values.len());
		stack_a.len = values.len();
		for (val, (idx, _)) in mapped.drain(..).enumerate()
		{
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
		if !ops { return }
		println!("Ops\n---");
		for op in &self.ops {
			println!("{op}")
		}
	}

	fn op(&mut self, op: Op, save: bool)
	{
		if save
		{
			self.saves.push(SaveState{
				stack_a: self.stack_a.clone(),
				stack_b: self.stack_b.clone(),
			});
		}

		match op {
			Op::Pa => {
				if self.stack_b.len() != 0
				{
					let v = *self.stack_b.front();
					self.stack_b.pop_front();
					self.stack_a.push_front(v);
				}
			},
			Op::Pb => {
				if self.stack_a.len() != 0
				{
					let v = *self.stack_a.front();
					self.stack_a.pop_front();
					self.stack_b.push_front(v);
				}
			}
			Op::Sa => {
				if self.stack_a.len() >= 2
				{
					let first = self.stack_a[0];
					let second = self.stack_a[1];
					self.stack_a[0] = second;
					self.stack_a[1] = first;
				}
			},
			Op::Sb => {
				if self.stack_b.len() >= 2
				{
					let first = self.stack_b[0];
					let second = self.stack_b[1];
					self.stack_b[0] = second;
					self.stack_b[1] = first;
				}
			},
			Op::Ra => {
				if self.stack_a.len() != 0
				{
					let v = *self.stack_a.front();
					self.stack_a.pop_front();
					self.stack_a.push_back(v);
				}
			},
			Op::Rb => {
				if self.stack_b.len() != 0
				{
					let v = *self.stack_b.front();
					self.stack_b.pop_front();
					self.stack_b.push_back(v);
				}
			},
			Op::Rr => {
				if self.stack_a.len() != 0
				{
					let v = *self.stack_a.front();
					self.stack_a.pop_front();
					self.stack_a.push_back(v);
				}
				if self.stack_b.len() != 0
				{
					let v = *self.stack_b.front();
					self.stack_b.pop_front();
					self.stack_b.push_back(v);
				}
			},
			Op::Rra => {
				if self.stack_a.len() != 0
				{
					let v = *self.stack_a.back();
					self.stack_a.pop_back();
					self.stack_a.push_front(v);
				}
			},
			Op::Rrb => {
				if self.stack_b.len() != 0
				{
					let v = *self.stack_b.back();
					self.stack_b.pop_back();
					self.stack_b.push_front(v);
				}
			},
			Op::Rrr => {
				if self.stack_a.len() != 0
				{
					let v = *self.stack_a.back();
					self.stack_a.pop_back();
					self.stack_a.push_front(v);
				}
				if self.stack_b.len() != 0
				{
					let v = *self.stack_b.back();
					self.stack_b.pop_back();
					self.stack_b.push_front(v);
				}
			},
		}
		self.ops.push(op);
	}
}

fn main() {
	let mut s = State::new(&[3, 1 , 5]);
	//s.op(Op::Sa, false);
	s.print(true);
}
