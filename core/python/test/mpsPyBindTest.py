# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import unittest

from projectaria_tools.core import mps

TEST_FOLDER = os.getenv("TEST_FOLDER")


closed_loop_trajectory_file = os.path.join(
    TEST_FOLDER, "mps_sample/trajectory/closed_loop_trajectory.csv"
)
open_loop_trajectory_file = os.path.join(
    TEST_FOLDER, "mps_sample/trajectory/open_loop_trajectory.csv"
)


class MPSTrajectory(unittest.TestCase):
    """
    Tests for reading trajectory files
    """

    def test_open_loop(self) -> None:
        mps_trajectory = mps.read_open_loop_trajectory(open_loop_trajectory_file)
        assert len(mps_trajectory) > 0

    def test_open_loop_invalid_file(self) -> None:
        mps_trajectory = mps.read_open_loop_trajectory("")
        assert len(mps_trajectory) == 0

    def test_closed_loop(self) -> None:
        mps_trajectory = mps.read_closed_loop_trajectory(closed_loop_trajectory_file)
        assert len(mps_trajectory) > 0

    def test_closed_loop_invalid_file(self) -> None:
        mps_trajectory = mps.read_closed_loop_trajectory("")
        assert len(mps_trajectory) == 0


global_points_file = os.path.join(
    TEST_FOLDER, "mps_sample/trajectory/global_points.csv.gz"
)
semidense_observations_file = os.path.join(
    TEST_FOLDER, "mps_sample/trajectory/semidense_observations.csv.gz"
)


class MPSPointCloudAndObservations(unittest.TestCase):
    """
    Tests for reading point cloud and observations from csv.gz files
    """

    def test_global_points(self) -> None:
        mps_global_points = mps.read_global_point_cloud(
            global_points_file, mps.StreamCompressionMode.GZIP
        )
        assert len(mps_global_points) > 0

        mps_global_points2 = mps.read_global_point_cloud(global_points_file)
        assert len(mps_global_points2) > 0

    def test_global_points_invalid_file(self) -> None:
        mps_global_points = mps.read_global_point_cloud("")
        assert len(mps_global_points) == 0

    def test_semidense_observations(self) -> None:
        mps_semidense_observations = mps.read_point_observations(
            semidense_observations_file, mps.StreamCompressionMode.GZIP
        )
        assert len(mps_semidense_observations) > 0

        mps_semidense_observations2 = mps.read_point_observations(
            semidense_observations_file
        )
        assert len(mps_semidense_observations2) > 0

    def test_semidense_observations_invalid_file(self) -> None:
        mps_semidense_observations = mps.read_point_observations("")
        assert len(mps_semidense_observations) == 0


online_calibration_file = os.path.join(
    TEST_FOLDER, "mps_sample/trajectory/online_calibration.jsonl"
)


class MPSOnlineCalibration(unittest.TestCase):
    """
    Tests for reading online calibration from jsonl files
    """

    def test_online_calibration(self) -> None:
        mps_online_calibration = mps.read_online_calibration(online_calibration_file)
        assert len(mps_online_calibration) > 0

    def test_online_calibration_invalid_file(self) -> None:
        mps_online_calibration = mps.read_online_calibration("")
        assert len(mps_online_calibration) == 0


class MPSEyeGaze(unittest.TestCase):
    """
    Tests for reading eye gaze data from csv files
    """

    def test_eyegaze_valid_file(self) -> None:
        eye_gaze_file = os.path.join(TEST_FOLDER, "mps_sample/eye_gaze/eyegaze.csv")
        mps_eye_gazes = mps.read_eyegaze(eye_gaze_file)
        self.assertGreater(len(mps_eye_gazes), 0)
        self.assertEqual(mps_eye_gazes[0].session_uid, "")

    def test_eyegaze_valid_file_with_session_id(self) -> None:
        eye_gaze_file = os.path.join(
            TEST_FOLDER, "mps_sample/eye_gaze/generalized_eye_gaze.csv"
        )
        mps_eye_gazes = mps.read_eyegaze(eye_gaze_file)
        self.assertGreater(len(mps_eye_gazes), 0)
        self.assertNotEqual(mps_eye_gazes[0].session_uid, "")

    def test_eyegaze_invalid_file(self) -> None:
        eye_gaze_file = ""
        mps_eye_gazes = mps.read_eyegaze(eye_gaze_file)
        self.assertEqual(len(mps_eye_gazes), 0)
