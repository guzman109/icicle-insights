--DROP TABLE IF EXISTS container_repositories;
--DROP TABLE IF EXISTS container_accounts;
--DROP TABLE IF EXISTS container_platforms;
DROP TABLE IF EXISTS github_repositories;
DROP TABLE IF EXISTS github_accounts;

--CREATE TABLE container_platforms (
--    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
--    name VARCHAR(255) NOT NULL UNIQUE,
--    pulls BIGINT DEFAULT 0,
--    stars INT DEFAULT 0,
--    created_at TIMESTAMPTZ DEFAULT NOW(),
--    updated_at TIMESTAMPTZ DEFAULT NOW(),
--    deleted_at TIMESTAMPTZ
--);
--
--CREATE TABLE container_accounts (
--    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
--    name VARCHAR(255) NOT NULL,
--    platform_id UUID NOT NULL REFERENCES container_platforms(id),
--    pulls BIGINT DEFAULT 0,
--    stars INT DEFAULT 0,
--    created_at TIMESTAMPTZ DEFAULT NOW(),
--    updated_at TIMESTAMPTZ DEFAULT NOW(),
--    deleted_at TIMESTAMPTZ,
--    UNIQUE(name, platform_id)
--);
--
--CREATE TABLE container_repositories (
--    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
--    name VARCHAR(255) NOT NULL,
--    account_id UUID NOT NULL REFERENCES container_accounts(id),
--    pulls BIGINT DEFAULT 0,
--    stars INT DEFAULT 0,
--    created_at TIMESTAMPTZ DEFAULT NOW(),
--    updated_at TIMESTAMPTZ DEFAULT NOW(),
--    deleted_at TIMESTAMPTZ,
--    UNIQUE(name, account_id)
--);

CREATE TABLE github_accounts (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name VARCHAR(255) NOT NULL,
    followers INT DEFAULT 0,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW(),
    deleted_at TIMESTAMPTZ,
    UNIQUE(name)
);

CREATE TABLE github_repositories (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name VARCHAR(255) NOT NULL,
    account_id UUID NOT NULL REFERENCES github_accounts(id),
    clones INT DEFAULT 0,
    forks INT DEFAULT 0,
    stars INT DEFAULT 0,
    subscribers INT DEFAULT 0,
    views BIGINT DEFAULT 0,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW(),
    deleted_at TIMESTAMPTZ,
    UNIQUE(name, account_id)
);
