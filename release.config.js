module.exports = {
  branches: [
    { name: 'main', prerelease: 'alpha' },
    // semantic-release requires at least one release branch in the config. `stable` exists
    // on the remote but is never pushed to: all releases are prereleases from `main`.
    'stable',
  ],
  tagFormat: 'v${version}',
  plugins: [
    [
      '@semantic-release/commit-analyzer',
      {
        preset: 'conventionalcommits',
        releaseRules: [
          { type: 'feat', release: 'minor' },
          { type: 'fix', release: 'patch' },
          { type: 'perf', release: 'patch' },
        ],
      },
    ],
    [
      '@semantic-release/release-notes-generator',
      {
        preset: 'conventionalcommits',
        presetConfig: {
          types: [
            { type: 'feat', section: 'Features' },
            { type: 'fix', section: 'Bug Fixes' },
            { type: 'perf', section: 'Performance Improvements' },
            { type: 'build', hidden: true },
            { type: 'chore', hidden: true },
            { type: 'ci', hidden: true },
            { type: 'docs', hidden: true },
            { type: 'refactor', hidden: true },
            { type: 'revert', hidden: true },
            { type: 'style', hidden: true },
            { type: 'test', hidden: true },
          ],
        },
      },
    ],
    ['@semantic-release/changelog', { changelogFile: 'CHANGELOG.md' }],
    // Single source of truth for the version: version.txt. CMake reads it at configure time;
    // the publish-npm job copies it into client/npm/package.json at publish time.
    ['@semantic-release/exec', { prepareCmd: 'echo "${nextRelease.version}" > version.txt' }],
    [
      '@semantic-release/git',
      {
        assets: ['version.txt', 'CHANGELOG.md'],
        message: 'chore(release): ${nextRelease.version} [skip ci]',
      },
    ],
    '@semantic-release/github',
  ],
};
