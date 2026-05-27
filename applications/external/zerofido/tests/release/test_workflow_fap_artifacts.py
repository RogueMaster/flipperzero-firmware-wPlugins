from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
WORKFLOWS = ROOT / ".github" / "workflows"


class WorkflowFapArtifactTests(unittest.TestCase):
    def test_build_profiles_uploads_only_packaged_faps(self):
        workflow = (WORKFLOWS / "build-profiles.yml").read_text(encoding="utf-8")

        self.assertIn("dist/zerofido.fap", workflow)
        upload_section = workflow.split("- name: Upload FAP artifacts", 1)[1]
        self.assertNotIn("dist/zerofido.fap", upload_section)
        self.assertIn("dist/zerofido-${{ matrix.profile }}-release.fap", upload_section)

    def test_release_job_does_not_stage_or_upload_raw_profile_faps(self):
        workflow = (WORKFLOWS / "release.yml").read_text(encoding="utf-8")

        self.assertNotIn('cp dist/zerofido.fap "release/zerofido-${profile}.fap"', workflow)
        self.assertIn("release/zerofido-*-release.fap", workflow)
        self.assertNotIn("release/zerofido-*.fap", workflow)


if __name__ == "__main__":
    unittest.main()
