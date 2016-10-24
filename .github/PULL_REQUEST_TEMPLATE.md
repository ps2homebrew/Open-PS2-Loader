Checklists for Pull requests
----------------------------

About pull request itself:
- [X] I am submitting a pull request:)
- [ ] My submission does one logical thing only (one bugfix, one new feature). If I will want to supply multiple logical changes, I will submit multiple pull requests.

Code quality:
(not applicable for non-code fixes of course)
- [ ] My submission follow coding style of file (or follow this coding style guide <https://www.kernel.org/doc/Documentation/CodingStyle>)
- [ ] My submission is passing local test suite

Commits:
- [ ] My commits are logical, easily readable, with concise comments.
- [ ] My commits follow the KISS principle: do one thing, and do it well.

Licensing:
- [ ] I am the author of submission or have been authorized by submission copyright holder to issue this pull request.

Branching:
- [ ] My submission is based on master branch.
- [ ] My submission is compatible with latest master branch updates (no conflicts, I did a rebase if it was necessary).
- [ ] The name of the branch I want to merge upstream is not 'master' (except for only the most trivial fixes, like typos and such).
- [ ] My branch name is *feature/my-shiny-new-opl-feature-title* (for new features).
- [ ] My branch name is *bugfix/my-totally-non-hackish-workaround* (for bugfixes).
- [ ] My branch name is *doc/what-i-did-to-documentation* (for documentation updates).

Continuous integration:
- [x] [TODO] Once I will submit this pull request, I will wait for Travis-CI report (normally a couple of minutes) and fix any issues I might have introduced.
- [x] [TODO] Until TravisCI not is configured, you can configure and run TravisCI <https://travis-ci.org/>, it alrealdy have a working template.



Pull request description
------------------------
