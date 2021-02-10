flashy:	flashy.swift
	swiftc -g -v $< -o $@

clean:
	-rm -rf flashy flashy.dSYM
