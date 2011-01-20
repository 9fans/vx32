int	oscmpswap(long* addr, long oldval, long newval)
{
	return __sync_bool_compare_and_swap(addr, oldval, newval);
}

