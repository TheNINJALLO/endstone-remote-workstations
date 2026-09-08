"""Adversarial release-gate tests; never write synthetic qualification reports."""
import copy
import unittest
import scope

class ScopeGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.baseline,cls.reports=scope.check()

    def test_unqualified_scope_does_not_pass(self):
        reports=copy.deepcopy(self.reports)
        for report in reports:
            for row in report['entries']:
                row['acceptance']='not-qualified'
            for value in report['extra_surfaces'].values():
                value['acceptance']='not-qualified'
        entries,surfaces=scope.incomplete(reports)
        self.assertEqual((len(entries),len(surfaces)),(138,20))

    def test_green_labels_cannot_qualify_unimplemented_behavior(self):
        reports=copy.deepcopy(self.reports)
        for report in reports:
            report['all_ui_acceptance']='qualified'
            for row in report['entries']:
                row['acceptance']='qualified'
                for mode in row['modes'].values():
                    mode.pop('qualification_records',None)
                    mode['adapter']='implemented'
                    mode['checks']={k:'passed' for k in mode['checks']}
            for surface in report['extra_surfaces'].values():
                surface.pop('qualification_records',None)
                surface.update(acceptance='qualified',implementation='implemented',client_test='passed')
        entries,surfaces=scope.incomplete(reports)
        self.assertEqual((len(entries),len(surfaces)),(138,20))

    def test_alias_command_permission_and_source_drift_fail(self):
        for field,value in [('aliases',[]),('commands',[]),('permission',''),('source_kind','virtual'),('historical_status','unsupported'),('family','other')]:
            with self.subTest(field=field):
                report=copy.deepcopy(self.reports[0]);report['entries'][0][field]=value
                with self.assertRaises(AssertionError):
                    scope.validate_report(self.baseline,report,'windows-x64')

    def test_platform_baseline_missing_and_duplicated_scope_fail(self):
        for mutate in [lambda r:r.update(platform='linux-x64'),lambda r:r.update(baseline_sha256='0'*64),lambda r:r['entries'].pop(),lambda r:r['entries'].append(r['entries'][0]),lambda r:r['extra_surfaces'].pop('forms'),lambda r:r['entries'][0]['modes'].pop('real-source')]:
            report=copy.deepcopy(self.reports[0]);mutate(report)
            with self.assertRaises(AssertionError):
                scope.validate_report(self.baseline,report,'windows-x64')

    def test_frozen_captures_and_build_logs_are_not_interactive_evidence(self):
        for path in ['research/original-source/tests/fixtures/helper-ender-2169.json','research/native-evidence/linux-404aa91-sanitizer-tests.txt','../private.json','research/native-evidence/interactive/../../../original-ui-catalog.json']:
            with self.subTest(path=path),self.assertRaises(AssertionError):
                scope.evidence(self.reports[0],'chest','transient-virtual',{'path':path,'sha256':'0'*64})

if __name__=='__main__':
    unittest.main()
