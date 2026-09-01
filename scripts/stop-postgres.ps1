param(
    [string]$ComposeFile = "docker-compose.postgres.yml"
)

$ErrorActionPreference = "Stop"

Write-Host "Stopping PostgreSQL..."
docker compose -f $ComposeFile down
Write-Host "PostgreSQL stopped."
