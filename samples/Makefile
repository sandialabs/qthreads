build:
	@for a in $$(ls); do \
                if [ -d $$a ]; then \
                echo "building sample: $$a"; \
                $(MAKE) -C $$a; \
                fi; \
        done;
	@echo "Finished making samples!"

test: build
	@for a in $$(ls); do \
                if [ -d $$a ]; then \
                echo "testing sample: $$a"; \
                $(MAKE) test$$a -C $$a; \
                fi; \
        done;
	@echo "Finished testing samples!"

clean:
	@for a in $$(ls); do \
                if [ -d $$a ]; then \
                echo "cleaning sample: $$a"; \
                $(MAKE) clean -C $$a; \
                fi; \
        done;
	@echo "Finished cleaning samples!"

