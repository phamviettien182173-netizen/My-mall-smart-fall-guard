CREATE TABLE IF NOT EXISTS devices (
    device_id VARCHAR(64) PRIMARY KEY,
    status VARCHAR(20) DEFAULT 'OFFLINE',
    last_seen TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS telemetry_logs (
    id SERIAL PRIMARY KEY,
    device_id VARCHAR(64) REFERENCES devices(device_id),
    svm FLOAT,
    pitch FLOAT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS fall_events (
    id SERIAL PRIMARY KEY,
    device_id VARCHAR(64) REFERENCES devices(device_id),
    event_type VARCHAR(50),
    svm_peak FLOAT,
    is_resolved BOOLEAN DEFAULT FALSE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);