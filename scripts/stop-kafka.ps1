param(
    [string]$ComposeFile = "docker-compose.kafka.yml"
)

$ErrorActionPreference = "Stop"

Write-Host "Stopping Kafka..."
docker compose -f $ComposeFile down
Write-Host "Kafka stopped."
