#!/usr/bin/python3

import os

import abrt_p2_testing
from abrt_p2_testing import (create_fully_initialized_problem,
                             Problems2Entry)


class TestDeleteElements(abrt_p2_testing.TestCase):

    def setUp(self):
        self.p2_entry_path = create_fully_initialized_problem(self, self.p2)

    def tearDown(self):
        self.p2.DeleteProblems([self.p2_entry_path])

    def test_delete_elements(self):
        p2e = Problems2Entry(self.bus, self.p2_entry_path)

        deleted_elements = { "delete_one" : "delete one",
                             "delete_two" : "delete two",
                             "delete_six" : "delete six" }

        p2e.SaveElements(deleted_elements, 0x0)
        elements = p2e.getproperty("Elements")
        for e in deleted_elements.keys():
            self.assertIn(e, elements, "test element does not exist")

        p2e.DeleteElements(["delete_one"])
        elements = p2e.getproperty("Elements")
        self.assertNotIn("delete_one", elements, "'delete_one' has not been removed")
        self.assertIn("delete_two", elements, "the other elements have disappeared")
        self.assertIn("delete_six", elements, "the other elements have disappeared")

        p2e.DeleteElements(["delete_one", "delete_two", "delete_six"])
        elements = p2e.getproperty("Elements")
        self.assertNotIn("delete_one", elements)
        self.assertNotIn("delete_two", elements)
        self.assertNotIn("delete_six", elements)

        p2e.DeleteElements([])

        for path in ["/tmp/shadow", "/tmp/passwd"]:
            with open(path, "w") as tmp_file:
                tmp_file.write("should not be touched")

        resp = p2e.DeleteElements(["/tmp/shadow", "../../../../tmp/passwd"])

        try:
            os.unlink("/tmp/shadow")
        except OSError as ex:
            self.fail("removed an absolute path: %s" % (str(ex)))

        try:
            os.unlink("/tmp/passwd")
        except OSError as ex:
            self.fail("removed a relative path: %s" % (str(ex)))


if __name__ == "__main__":
    abrt_p2_testing.main(TestDeleteElements)
