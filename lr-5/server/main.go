package main

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"io"
	"log/slog"
	"net"
	"net/http"
	"net/netip"
	"os"
	"os/signal"
	"strconv"
	"strings"
	"sync/atomic"
	"time"
)

var (
	errConfig = errors.New("invalid configuration")
	errListen = errors.New("listen failed")
)

type config struct {
	host string
	port int
	args []string
}

func main() {
	var cfg config
	flag.StringVar(&cfg.host, "host", "127.0.0.1", "IPv4 address to listen on")
	flag.IntVar(&cfg.port, "port", 20112, "port in the range 20000–29999")
	flag.Parse()
	cfg.args = flag.Args()

	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt)
	defer stop()

	if err := run(ctx, cfg); err != nil {
		slog.ErrorContext(ctx, "server failed", slog.Any("error", err))
		switch {
		case errors.Is(err, errConfig):
			os.Exit(2)
		case errors.Is(err, errListen):
			os.Exit(3)
		default:
			os.Exit(1)
		}
	}
}

func run(ctx context.Context, cfg config) error {
	if len(cfg.args) != 0 {
		return fmt.Errorf("%w: unexpected positional arguments: %q", errConfig, cfg.args)
	}
	ip, err := netip.ParseAddr(cfg.host)
	if err != nil || !ip.Is4() || cfg.port < 20000 || cfg.port > 29999 {
		return fmt.Errorf("%w: provide an IPv4 address and a port in the range 20000–29999", errConfig)
	}

	server := &http.Server{
		Addr:              net.JoinHostPort(cfg.host, strconv.Itoa(cfg.port)),
		Handler:           newHandler(),
		ReadHeaderTimeout: 5 * time.Second,
		ReadTimeout:       10 * time.Second,
		WriteTimeout:      10 * time.Second,
		IdleTimeout:       10 * time.Second,
	}
	listener, err := net.Listen("tcp4", server.Addr)
	if err != nil {
		return fmt.Errorf("%w: %w", errListen, err)
	}
	serveResult := make(chan error, 1)
	go func() { serveResult <- server.Serve(listener) }()
	slog.InfoContext(ctx, "server started", slog.Int("pid", os.Getpid()), slog.String("address", listener.Addr().String()))

	select {
	case err := <-serveResult:
		if !errors.Is(err, http.ErrServerClosed) {
			return fmt.Errorf("serve: %w", err)
		}
	case <-ctx.Done():
		shutdownCtx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer cancel()
		if err := server.Shutdown(shutdownCtx); err != nil {
			if closeErr := server.Close(); closeErr != nil {
				return fmt.Errorf("shutdown: %w; close: %v", err, closeErr)
			}
			return fmt.Errorf("shutdown: %w", err)
		}
		if err := <-serveResult; !errors.Is(err, http.ErrServerClosed) {
			return fmt.Errorf("serve: %w", err)
		}
	}
	slog.InfoContext(ctx, "server stopped")
	return nil
}

func newHandler() http.Handler {
	small := strings.Repeat("a", 1024)
	large := strings.Repeat("b", 1024*1024)
	var requests atomic.Uint64

	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		id := requests.Add(1)
		ctx := r.Context()
		logger := slog.With(
			slog.Uint64("requestID", id),
			slog.String("peer", r.RemoteAddr),
			slog.String("method", r.Method),
			slog.String("path", r.URL.Path),
		)
		status := http.StatusOK
		body := ""

		if r.Method != http.MethodGet && r.Method != http.MethodHead {
			status = http.StatusMethodNotAllowed
			body = "method not allowed\n"
			w.Header().Set("Allow", "GET, HEAD")
		} else {
			switch r.URL.Path {
			case "/small":
				body = small
			case "/large":
				body = large
			case "/empty":
			case "/chunked":
				body = small
			case "/slow":
				select {
				case <-time.After(time.Second):
					body = small
				case <-ctx.Done():
					logger.InfoContext(ctx, "request cancelled")
					return
				}
			default:
				status = http.StatusNotFound
				body = "not found\n"
			}
		}

		w.Header().Set("Content-Type", "text/plain; charset=utf-8")
		if r.Method == http.MethodGet && r.URL.Path == "/chunked" {
			w.Header().Set("Transfer-Encoding", "chunked")
		} else {
			w.Header().Set("Content-Length", strconv.Itoa(len(body)))
		}
		w.WriteHeader(status)
		if r.Method != http.MethodHead {
			if _, err := io.WriteString(w, body); err != nil {
				logger.ErrorContext(ctx, "response write failed", slog.Int("status", status), slog.Any("error", err))
				return
			}
		}
		logger.InfoContext(ctx, "request completed", slog.Int("status", status))
	})
}
