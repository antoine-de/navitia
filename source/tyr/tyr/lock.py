# Copyright (c) 2001-2014, Canal TP and/or its affiliates. All rights reserved.
#
# This file is part of Navitia,
#     the software to build cool stuff with public transport.
#
# Hope you'll enjoy and contribute to this project,
#     powered by Canal TP (www.canaltp.fr).
# Help us simplify mobility and open public transport:
#     a non ending quest to the responsive locomotion way of traveling!
#
# LICENCE: This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#
# Stay tuned using
# twitter @navitia
# IRC #navitia on freenode
# https://groups.google.com/d/forum/navitia
# www.navitia.io
from IPython.core import logger

from tyr import redis
from tyr.helper import get_instance_logger


class retry_wrapper:
    """
    default call_on_fail wrapper, retry the celery task
    """
    def __init__(self, obj, countdown=2, max_retries=100):
        self.obj = obj
        self.countdown = countdown
        self.max_retries = max_retries

    def __call__(self):
        print "retying"
        self.obj.retry(countdown=self.countdown, max_retries=self.max_retries)


class Lock:
    def __enter__(self):
        self.lock = redis.lock('tyr.lock|' + self.instance, timeout=self.lock_timeout)
        print "on entre dans le lock"
        if not self.lock.acquire(blocking=False):
            print ("cannot acquire lock, retrying")
            self.call_on_fail_function()

        print ("lock acquired for instance {}".format(self.instance))

    def __exit__(self, exc_type, exc_value, traceback):
        print 'Releasing lock'
        self.lock.release()

    def __init__(self, instance, call_on_fail, lock_timeout=60*120):
        print 'Constructing Lock'
        self.instance = instance
        self.lock_timeout = lock_timeout
        self.call_on_fail_function = call_on_fail