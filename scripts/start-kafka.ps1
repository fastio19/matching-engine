param(
    [string]$ComposeFile = "docker-compose.kafka.yml",
    [int]$Partitions = 12
)

$ErrorActionPreference = "Stop"

Write-Host "Starting Kafka with Docker Compose..."
docker compose -f $ComposeFile up -d

Write-Host "Waiting for Kafka to become ready..."
$ready = $false
for ($i = 0; $i -lt 30; $i++) {
    try {
        docker compose -f $ComposeFile exec -T kafka bash -lc "kafka-topics --bootstrap-server localhost:9092 --list" | Out-Null
        Write-Host "Kafka is ready."
        $ready = $true
        break
    }
    catch {
        Start-Sleep -Seconds 2
    }
}

if (-not $ready) {
    throw "Kafka did not become ready within the wait window."
}

Write-Host "Creating topics..."
$topics = @("orders.commands", "orders.trades", "orders.book", "orders.dlq")
foreach ($topic in $topics) {
    docker compose -f $ComposeFile exec -T kafka bash -lc "kafka-topics --bootstrap-server localhost:9092 --create --if-not-exists --topic $topic --partitions $Partitions --replication-factor 1" | Out-Null
    docker compose -f $ComposeFile exec -T kafka bash -lc "kafka-topics --bootstrap-server localhost:9092 --alter --topic $topic --partitions $Partitions" | Out-Null
}

Write-Host "Kafka setup complete."
