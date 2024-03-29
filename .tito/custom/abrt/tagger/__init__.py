import re
from fileinput import FileInput

from tito.common import get_latest_tagged_version, run_command
from tito.tagger import VersionTagger


class AbrtVersionTagger(VersionTagger):
    CHANGELOG_FILE = "CHANGELOG.md"

    def _update_changelog(self, new_version: str):
        """
        Update changelog with the new version. This entails renaming headings
        in the Markdown file and updating links to compare the corresponding
        commits on GitHub.
        """
        # Update the %changelog section in the spec file.
        super()._update_changelog(new_version)

        # Drop the RPM release number, e.g. 2.14.6-1 → 2.14.6.
        new_version = new_version[:new_version.find("-")]

        with FileInput(self.CHANGELOG_FILE, inplace=True) as changelog:
            for line in changelog:
                if line.startswith("## [Unreleased]"):
                    # Add a heading for the release right below "Unreleased",
                    # inheriting its contents. This means that changes that were
                    # unreleased until now have become released in this new version.
                    line += f"\n## [{new_version}]\n"
                elif line.startswith("[Unreleased]:"):
                    # Update link to comparison of changes on GitHub.
                    match = re.search(r"(https://.+/compare/)(.+)\.\.\.HEAD", line)
                    assert match is not None
                    url_prefix = match[1]
                    old_version = match[2]
                    line = (f"[Unreleased]: {url_prefix}{new_version}...HEAD\n"
                            f"[{new_version}]: {url_prefix}{old_version}...{new_version}\n")

                print(line, end="")

        run_command(f"git add -- {self.CHANGELOG_FILE}")
