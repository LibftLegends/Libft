# Compatibility entry point.  The root Libft Makefile owns the complete test
# object and archive graph; this file must not create a second dependency
# scanner, progress counter, lock, or recursive Libft build.

LIBFT_ROOT_DIR := ..

.PHONY: all debug both build_tests clean fclean re \
	Basic Time CMA Efficiency performance_benchmarks run_performance_benchmarks \
	run_Efficiency get_next_line File Config Filesystem Storage Voxel CLI \
	Application Logger Sink GPGR single_test run_single_test \
	run-tests \
	print_test_compile_flags print_test_ldflags print_test_objdir

all build_tests Basic Time CMA get_next_line File Config Filesystem Storage \
Voxel CLI Application Logger Sink GPGR:
	+$(MAKE) -C $(LIBFT_ROOT_DIR) tests

debug:
	+$(MAKE) -C $(LIBFT_ROOT_DIR) debug-tests

both:
	+$(MAKE) -C $(LIBFT_ROOT_DIR) tests debug-tests

Efficiency performance_benchmarks:
	+$(MAKE) -C $(LIBFT_ROOT_DIR) performance_benchmarks

run_performance_benchmarks run_Efficiency:
	+$(MAKE) -C $(LIBFT_ROOT_DIR) run_performance_benchmarks

single_test:
	@if [ -z "$(TEST)" ]; then \
		echo "Usage: make single_test TEST=Test/test_name.cpp"; \
		exit 1; \
	fi
	+python3 tools/build_single_test.py "$(TEST)"

run_single_test: single_test

run-tests:
	+$(MAKE) -C $(LIBFT_ROOT_DIR) run-tests

clean:
	+$(MAKE) -C $(LIBFT_ROOT_DIR) clean

fclean:
	+$(MAKE) -C $(LIBFT_ROOT_DIR) fclean

re:
	+$(MAKE) -C $(LIBFT_ROOT_DIR) re-tests

print_test_compile_flags:
	+$(MAKE) -C $(LIBFT_ROOT_DIR) -s print-build-mode

print_test_ldflags:
	+$(MAKE) -C $(LIBFT_ROOT_DIR) -s print-build-mode

print_test_objdir:
	+$(MAKE) -C $(LIBFT_ROOT_DIR) -s print-build-mode
