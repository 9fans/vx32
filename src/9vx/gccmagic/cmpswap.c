int	oscmpswap(long* addr, long oldval, long newval)
{
	int res;
	res = __sync_bool_compare_and_swap(addr, oldval, newval);
	if (res) 
		return 1;
	return 0;
}

