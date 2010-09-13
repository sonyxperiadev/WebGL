<?php
# Copyright (C) 2010 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

###############################################################################

# Show the source of the test.
#
# Usage:
#   view_source.php?src=PATH
#   where
#     PATH - relative path in the LayoutTests dir

  # Global variables
  # The server document root is LayoutTests/http/tests. See run_apache2.py.
  $rootDir = realpath($_SERVER['DOCUMENT_ROOT'] . '..' . DIRECTORY_SEPARATOR . '..');

  function getAbsolutePath($relPath) {
    global $rootDir;
    return $rootDir . DIRECTORY_SEPARATOR . $relPath;
  }

  function main() {
    global $rootDir;

    # Very primitive check if path tries to go above DOCUMENT_ROOT or is absolute
    if (strpos($_GET['src'], "..") !== False ||
        substr($_GET['src'], 0, 1) == DIRECTORY_SEPARATOR) {
      return;
    }

    # If we don't want realpath to append any prefixes we need to pass it an absolute path
    $src = realpath(getAbsolutePath($_GET['src']));

    echo "<html><body>";
    # TODO: Add link following and syntax highlighting for html and js.
    highlight_string(file_get_contents($src));
    echo "</body></html>";
  }

  main();
?>
