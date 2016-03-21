# when called without a target, do 'make all' in src
.DEFAULT_GOAL := all

# for each target (% being a wildcard), pass the target ($@) to the src folder
%:
	$(MAKE) -C src $@
