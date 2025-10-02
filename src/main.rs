use std::{collections::{vec_deque, VecDeque}, fmt::Display, ops::{Index, IndexMut}, ptr};

use crate::stack_deque::StackDeque;
mod stack_deque;

#[derive(Debug, Clone, Copy, Eq, PartialEq)]
#[repr(u8)]
pub enum Op {
	Pa,
	Pb,
	Sa,
	Sb,
	Ra,
	Rb,
	Rr,
	Rra,
	Rrb,
	Rrr,
}

impl Display for Op
{
	fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
		match self
		{
			Op::Pa => write!(f, "pa"),
			Op::Pb => write!(f, "pb"),
			Op::Sa => write!(f, "sa"),
			Op::Sb => write!(f, "sb"),
			Op::Ra => write!(f, "ra"),
			Op::Rb => write!(f, "rb"),
			Op::Rr => write!(f, "rr"),
			Op::Rra => write!(f, "rra"),
			Op::Rrb => write!(f, "rrb"),
			Op::Rrr => write!(f, "rrr"),
		}
	}
}

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
		for (idx, val) in mapped
		{
			stack_a[idx] = val;
		}

		Self {
	 		stack_a: todo!(),
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
}

fn main() {
	let mut s = State::new(&[1, 3 , 5]);
	s.print(true);
}
