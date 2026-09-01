param(
    [string]$ComposeFile = "docker-compose.production.yml"
)

$ErrorActionPreference = "Stop"

docker compose -f $ComposeFile down
Write-Host "Matching engine stack stopped."
