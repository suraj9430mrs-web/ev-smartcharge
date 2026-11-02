# EV SmartCharge - Multi-stage Docker Build
# Stage 1: Build the C++ backend
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    clang \
    libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy backend source
COPY backend/ backend/

# Build the binary
RUN cd backend && \
    clang++ -std=c++17 -O2 -I lib \
    -o ev_smartcharge \
    main.cpp db/database.cpp services/charging_engine.cpp api/routes.cpp \
    -lsqlite3 -pthread

# Stage 2: Runtime image
FROM ubuntu:22.04 AS runtime

RUN apt-get update && apt-get install -y \
    libsqlite3-0 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy binary from builder
COPY --from=builder /app/backend/ev_smartcharge .

# Copy frontend assets
COPY frontend/ frontend/

# Create data directory
RUN mkdir -p /app/data

# Environment defaults
ENV PORT=8080
ENV DATABASE_PATH=/app/data/ev_smartcharge.db

EXPOSE 8080

# Health check
HEALTHCHECK --interval=30s --timeout=5s \
    CMD curl -f http://localhost:${PORT}/health || exit 1

CMD ["./ev_smartcharge"]

