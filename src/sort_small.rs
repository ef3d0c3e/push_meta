use core::panic;

use crate::{ops::Op, State};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum Block {
	BlkATop,
	BlkABot,
	BlkBTop,
	BlkBBot,
}

fn permutation_id(slice: &[i32]) -> usize
{
	let n = slice.len();
    assert!(n <= 5);

	let mut ranks = vec![0usize; n];
	for i in 0..n {
		let mut cnt = 0;
		for j in 0..n {
			if slice[j] < slice[i] {
				cnt += 1;
			}
		}
		ranks[i] = cnt;
	}

	// convert ranks to Lehmer code / permutation ID
	let mut id = 0;
	let mut factor = 1;
	for i in (0..n).rev() {
		id += ranks[i] * factor;
		factor *= n - i;
	}
	id
}

static SORT_2A : [&[Op]; 2] = [
    &[Op::Sa],
    &[],
];
static SORT_2B : [&[Op]; 2] = [
    &[Op::Sb, Op::Pa],
    &[],
];
static SORT_3A : [&[Op]; 6] = [
	// u > v > w
    &[Op::Sa, Op::Ra, Op::Sa, Op::Rra, Op::Sa],
	// u > w > v
    &[Op::Sa, Op::Ra, Op::Sa, Op::Rra],
	// v > u > w
    &[Op::Ra, Op::Sa, Op::Rra, Op::Sa],
	// v > w > u
    &[Op::Ra, Op::Sa, Op::Rra],
	// w > u > v
    &[Op::Sa],
	// w > v > u
    &[],
];
static SORT_3B : [&[Op]; 6] = [
    // u > v > w
    &[Op::Pa, Op::Pa, Op::Pa],
    // u > w > v
    &[Op::Pa, Op::Sb, Op::Pa, Op::Pa],
    // v > u > w
    &[Op::Sb, Op::Pa, Op::Pa, Op::Pa],
    // v > w > u
    &[Op::Sb, Op::Pa, Op::Sb, Op::Pa, Op::Pa],
    // w > u > v
    &[Op::Pa, Op::Sb, Op::Pa, Op::Sa, Op::Pa],
    // w > v > u
    &[Op::Sb, Op::Pa, Op::Sb, Op::Pa, Op::Sa, Op::Pa],
];

pub fn sort_small(state: &State, blk: Block, len: usize) -> &'static [Op]
{
	if len <= 1
	{
		if len == 1 && blk == Block::BlkBTop
		{
			return &[Op::Pa]
		}
		return &[]
	}

	let slice = match blk {
		Block::BlkATop => &state.stack_a[0..len],
		Block::BlkBTop => &state.stack_b[0..len],
		_ => panic!("Unsupported sort_small")
	};
	let id = permutation_id(slice);

	if blk == Block::BlkATop && len == 2
	{
		return SORT_2A[id];
	}
	else if blk == Block::BlkATop && len == 3
	{
		return SORT_3A[id];
	}
	if blk == Block::BlkBTop && len == 2
	{
		return SORT_2B[id];
	}
	else if blk == Block::BlkBTop && len == 3
	{
		return SORT_3B[id];
	}
	panic!("Unsupported sort_small")
}
