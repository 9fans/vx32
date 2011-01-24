int	oscmpswap(long* addr, long oldval, long newval)
{
	bool res;
	print("oscmpswap: addr %p, *addr %ld, oldval %ld, newval %ld\n", 
			addr, *addr, oldval, newval);
	res = __sync_bool_compare_and_swap(addr, oldval, newval);
	print("oscmpswap: result %d\n", res);
	if (res) 
		return 1;
	return 0;
}

