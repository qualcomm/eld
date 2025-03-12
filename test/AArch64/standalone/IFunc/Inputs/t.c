
static void func1_neon(void)
{
}

static void func1_vfp(void)
{
}

static void func1_arm(void)
{
}

/* This line makes func1 a GNU indirect function. */
asm (".type func1, %gnu_indirect_function");

void *func1(unsigned long int hwcap)
{
   if (hwcap & 4096) /*HWCAP_ARM_NEON*/
	return &func1_neon;
   else if (hwcap & 8192) /*HWCAP_ARM_VFP*/
	return &func1_vfp;
   else
	return &func1_arm;
}
