-- ICICLE Insights Database Schema

CREATE TABLE git_platforms (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name VARCHAR(255) NOT NULL UNIQUE,
    clones INT DEFAULT 0,
    followers INT DEFAULT 0,
    forks INT DEFAULT 0,
    stars INT DEFAULT 0,
    views BIGINT DEFAULT 0,
    watchers INT DEFAULT 0,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW(),
    deleted_at TIMESTAMPTZ
);

CREATE TABLE git_accounts (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name VARCHAR(255) NOT NULL,
    platform_id UUID NOT NULL REFERENCES git_platforms(id),
    clones INT DEFAULT 0,
    followers INT DEFAULT 0,
    forks INT DEFAULT 0,
    stars INT DEFAULT 0,
    views BIGINT DEFAULT 0,
    watchers INT DEFAULT 0,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW(),
    deleted_at TIMESTAMPTZ,
    UNIQUE(name, platform_id)
);

CREATE TABLE git_repositories (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name VARCHAR(255) NOT NULL,
    account_id UUID NOT NULL REFERENCES git_accounts(id),
    clones INT DEFAULT 0,
    followers INT DEFAULT 0,
    forks INT DEFAULT 0,
    stars INT DEFAULT 0,
    views BIGINT DEFAULT 0,
    watchers INT DEFAULT 0,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW(),
    deleted_at TIMESTAMPTZ,
    UNIQUE(name, account_id)
);
