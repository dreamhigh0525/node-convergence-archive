# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'copies_up',
      'type': 'none',
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/../copies-out-updir',
          'files': [
            'file1',
          ],
        },
      ],
    },
  ],
}
