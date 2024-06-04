
RPATH := $(shell echo $$MASTER_FISSLOCK_PATH)/experiments/results
RESULT_PATH := $(shell echo $$RESULT_PATH)

SYSTEM ?= fisslock
TEST ?= micro-rm-zipf

OPATH := $(RESULT_PATH)/$(TEST)

all: $(OPATH)/lat-$(SYSTEM)-grant-cdf

$(OPATH)/lat-$(SYSTEM)-grant: $(OPATH)/lat-$(SYSTEM)-raw
	bash $(RPATH)/parse-timer.sh $(SYSTEM) $(TEST)

$(OPATH)/lat-$(SYSTEM)-grant-cdf: $(OPATH)/lat-$(SYSTEM)-grant
	python3 $(RPATH)/get_cdf_data.py $(SYSTEM) $(TEST) $(RESULT_PATH)
