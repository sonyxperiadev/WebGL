# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from google.appengine.ext import db
from google.appengine.ext import webapp

from model.workitems import WorkItems
from model.activeworkitems import ActiveWorkItems
from model import queuestatus

from datetime import datetime, timedelta


class NextPatch(webapp.RequestHandler):
    def _get_next_patch_id(self, queue_name):
        work_items = WorkItems.all().filter("queue_name =", queue_name).get()
        if not work_items:
            return None
        active_work_items = ActiveWorkItems.get_or_insert(key_name=queue_name, queue_name=queue_name)
        return db.run_in_transaction(self._assign_patch, active_work_items.key(), work_items.item_ids)

    def get(self, queue_name):
        patch_id = self._get_next_patch_id(queue_name)
        if not patch_id:
            self.error(404)
            return
        self.response.out.write(patch_id)

    @staticmethod
    def _assign_patch(key, work_item_ids):
        now = datetime.now()
        active_work_items = db.get(key)
        active_work_items.deactivate_expired(now)
        next_item = active_work_items.next_item(work_item_ids, now)
        active_work_items.put()
        return next_item
