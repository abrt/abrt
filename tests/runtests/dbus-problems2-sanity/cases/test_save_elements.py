#!/usr/bin/python3
# vim: set makeprg=python3-flake8\ %

import os
import dbus

import abrt_p2_testing
from abrt_p2_testing import (create_fully_initialized_problem,
                             get_huge_file_path,
                             open_fd,
                             Problems2Entry,
                             get_dbus_limit_elements_count,
                             get_dbus_limit_data_size)


class TestSaveElements(abrt_p2_testing.TestCase):

    def setUp(self):
        self.p2_entry_path = create_fully_initialized_problem(self, self.p2)

    def tearDown(self):
        self.p2.DeleteProblems([self.p2_entry_path])

    def test_save_elements(self):
        p2e = Problems2Entry(self.bus, self.p2_entry_path)

        shorttext = "line one\nline two\nline three\n"
        with open("/tmp/shorttext", "w") as shorttext_file:
            shorttext_file.write(shorttext)

        elembytes = dbus.types.Array(
                bytearray([0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
                           0xA, 0xB, 0xC, 0xD, 0xE, 0xF]),
                "y")

        with open("/tmp/shorttext", "r") as fstab_file:
            request = {"random": "random text",
                       "shorttext": dbus.types.UnixFd(fstab_file),
                       "bytes": elembytes}

            p2e.SaveElements(request, 0)

        elements = p2e.getproperty("Elements")
        self.assertIn(
                "random",
                elements,
                "property 'elements' does not include the created elements")
        self.assertIn(
                "shorttext",
                elements,
                "property 'elements' does not include the created elements")
        self.assertIn(
                "bytes",
                elements,
                "property 'elements' does not include the created elements")

        # 0x20 because binary files (bytes) are returned as in the form of
        # File descriptor by default
        exp = {"random": "random text",
               "shorttext": shorttext,
               "bytes": elembytes}

        resp = p2e.ReadElements(exp.keys(), 0x20)
        self.assertDictContainsSubset(exp, resp)

        resp = p2e.SaveElements(dict(), 0x0)

        for path in ["/tmp/shadow", "/tmp/passwd"]:
            try:
                os.unlink(path)
            except OSError:
                pass

        self.assertRaisesDBusError(
            "org.freedesktop.DBus.Error.AccessDenied: "
            "Not allowed problem element name",
            p2e.SaveElements,
            {"/tmp/shadow": "blah"},
            0x0)

        try:
            os.unlink("/tmp/shadow")
            self.fail("accepted an absolute path")
        except OSError:
            pass

        self.assertRaisesDBusError(
            "org.freedesktop.DBus.Error.AccessDenied: "
            "Not allowed problem element name",
            p2e.SaveElements,
            {"../../../../tmp/passwd": "bar"},
            0x0)

        try:
            os.unlink("/tmp/passwd")
            self.fail("accepted a relative path")
        except OSError:
            pass

    def test_save_elements_count_limit(self):
        entry = Problems2Entry(self.bus, self.p2_entry_path)

        too_many_elements = dict()
        for i in range(get_dbus_limit_elements_count() + 1):
            too_many_elements[str(i)] = str(i)

        entry.SaveElements(too_many_elements, 0)
        elements = entry.getproperty("Elements")
        saved = False
        ignored = False
        for i in range(get_dbus_limit_elements_count() + 1):
            if str(i) in elements:
                saved = True
            else:
                ignored = True

        self.assertTrue(saved,
                        "SaveElements: saved as many as possible")
        self.assertTrue(ignored,
                        "SaveElements: did not save elements over limit")

        for i in range(get_dbus_limit_elements_count() + 1):
            key = str(get_dbus_limit_elements_count() + 1 + i)
            ed = {key: str(i + 1)}
            try:
                entry.SaveElements(ed, 0)
                data = entry.ReadElements([key], 0)
            except dbus.exceptions.DBusException as ex:
                print(key)
                raise ex
            self.assertNotIn(key,
                             data,
                             "SaveElements: did not create new elements")

        overwrites = False
        for i in range(get_dbus_limit_elements_count() + 1):
            ed = {str(i): str(i + 2)}
            try:
                entry.SaveElements(ed, 0)
                overwrites = True
            except dbus.exceptions.DBusException as ex:
                self.assertEqual(
                    "org.freedesktop.DBus.Error.LimitsExceeded: "
                    "Too many elements",
                    str(ex))
                break

        self.assertTrue(overwrites,
                        "SaveElements allows to overwrite element "
                        "despite Elements count limit")

    def test_save_elements_data_size_limit(self):
        huge_file_path = get_huge_file_path()

        entry = Problems2Entry(self.bus, self.p2_entry_path)

        key = "huge_file"
        with open(huge_file_path, "r") as huge_file:
            entry.SaveElements({key: dbus.types.UnixFd(huge_file)}, 0)
            data = entry.ReadElements([key], 0x1)
            self.assertIn(key, data, "SaveElements: created truncated file")
            fd = data[key].take()
            end = os.lseek(fd, 0, os.SEEK_END)
            os.close(fd)
            self.assertLessEqual(end,
                                 get_dbus_limit_data_size(),
                                 "SaveElements: wrote up to Size limit Bytes")

        smaller_ed = {key: "smaller file"}
        entry.SaveElements(smaller_ed, 0)
        data = entry.ReadElements([key], 0x4)
        self.assertIn(key, data, "SaveElements: created non-text file")
        self.assertEqual(smaller_ed[key],
                         data[key],
                         "SaveElements: dump directory does not grow")

    def test_non_readable_filedescriptor(self):
        entry = Problems2Entry(self.bus, self.p2_entry_path)

        with open_fd("/etc/passwd", os.O_PATH) as fd:
            entry.SaveElements({"passwd": dbus.types.UnixFd(fd)}, 0)
            data = entry.ReadElements(["passwd"], 0x0)
            self.assertNotIn("passwd", data)

    def test_pipes(self):
        entry = Problems2Entry(self.bus, self.p2_entry_path)

        rp, wp = os.pipe()
        try:
            entry.SaveElements({"pipe0": dbus.types.UnixFd(rp)}, 0)
            data = entry.ReadElements(["pipe0"], 0x0)

            self.assertNotIn("pipe0", data)

            os.write(wp, b"Epic success!")
            os.close(wp)
            wp = -1

            entry.SaveElements({"pipe1": dbus.types.UnixFd(rp)}, 0)
            data = entry.ReadElements(["pipe1"], 0x0)

            self.assertIn("pipe1", data)
            self.assertEqual(data["pipe1"], "Epic success!")
        finally:
            os.close(rp)
            if wp != -1:
                os.close(wp)


if __name__ == "__main__":
    abrt_p2_testing.main(TestSaveElements)
