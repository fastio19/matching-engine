param(
    [string]$ComposeFile = "docker-compose.production.yml"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path ".env")) {
    Copy-Item ".env.example" ".env"
    Write-Host "Created .env from .env.example. Review secrets before production use."
}

docker compose -f $ComposeFile up -d --build
Write-Host "Matching engine stack is starting."
Write-Host "Broker API health: http://localhost:8080/health"
