use std::fmt::Display;

#[derive(Debug, Clone, Copy, Eq, PartialEq)]
#[repr(u8)]
pub enum Op {
	Pa,
	Pb,
	Sa,
	Sb,
	Ss,
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
			Op::Ss => write!(f, "ss"),
			Op::Ra => write!(f, "ra"),
			Op::Rb => write!(f, "rb"),
			Op::Rr => write!(f, "rr"),
			Op::Rra => write!(f, "rra"),
			Op::Rrb => write!(f, "rrb"),
			Op::Rrr => write!(f, "rrr"),
		}
	}
}
