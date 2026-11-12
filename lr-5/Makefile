GO ?= go
SERVER_BIN ?= http-test-server

ifeq ($(OS),Windows_NT)
SERVER_BIN := $(SERVER_BIN).exe
endif

.PHONY: server run-server

server:
	$(GO) build -o "$(SERVER_BIN)" ./server/main.go

run-server:
	$(GO) run ./server/main.go
