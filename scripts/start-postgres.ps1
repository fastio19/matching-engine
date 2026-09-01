param(
    [string]$ComposeFile = "docker-compose.postgres.yml"
)

$ErrorActionPreference = "Stop"

Write-Host "Starting PostgreSQL with Docker Compose..."
docker compose -f $ComposeFile up -d

Write-Host "Waiting for PostgreSQL to become ready..."
$ready = $false
for ($i = 0; $i -lt 30; $i++) {
    try {
        docker compose -f $ComposeFile exec -T postgres pg_isready -U postgres -d matching_engine | Out-Null
        Write-Host "PostgreSQL is ready."
        $ready = $true
        break
    }
    catch {
        Start-Sleep -Seconds 2
    }
}

if (-not $ready) {
    throw "PostgreSQL did not become ready within the wait window."
}

Write-Host "PostgreSQL setup complete."
Write-Host "Connection string: host=localhost port=5432 dbname=matching_engine user=postgres password=postgres"
