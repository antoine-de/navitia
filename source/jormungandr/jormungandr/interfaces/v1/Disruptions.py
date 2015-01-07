# coding=utf-8

#  Copyright (c) 2001-2014, Canal TP and/or its affiliates. All rights reserved.
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

from flask.ext.restful import marshal_with, reqparse, abort
from jormungandr import i_manager, timezone
from fields import PbField, error, network, line,\
    NonNullList, NonNullNested, pagination, stop_area
from ResourceUri import ResourceUri
from jormungandr.interfaces.argument import ArgumentDoc
from jormungandr.interfaces.parsers import date_time_format
from errors import ManageError
from datetime import datetime
import aniso8601
from datetime import timedelta

disruption = {
    "network": PbField(network, attribute='network'),
    "lines": NonNullList(NonNullNested(line)),
    "stop_areas": NonNullList(NonNullNested(stop_area))
}

disruptions = {
    "disruptions": NonNullList(NonNullNested(disruption)),
    "error": PbField(error, attribute='error'),
    "pagination": NonNullNested(pagination)
}


def duration(param):
    try:
        return aniso8601.parse_duration(param)
    except ValueError as e:
        raise ValueError("Unable to parse duration {}, {}".format(param, e.message))


class Disruptions(ResourceUri):
    def __init__(self):
        ResourceUri.__init__(self)
        self.parsers = {}
        self.parsers["get"] = reqparse.RequestParser(
            argument_class=ArgumentDoc)
        parser_get = self.parsers["get"]
        parser_get.add_argument("depth", type=int, default=1)
        parser_get.add_argument("count", type=int, default=10,
                                description="Number of disruptions per page")
        parser_get.add_argument("start_page", type=int, default=0,
                                description="The current page")
        parser_get.add_argument("period", type=int,
                                description="Period in days from datetime. DEPRECATED use duration parameter")
        parser_get.add_argument("duration", type=duration, default=timedelta(days=365),
                                description="Duration of the period we want to display the disruption on")
        parser_get.add_argument("datetime", type=date_time_format, default=datetime.now(), #TODO!! we have to take the local instance time
                                description="The datetime from which you want the disruption "
                                            "(filter on the disruption application periods)")
        parser_get.add_argument("_current_datetime", type=date_time_format, default=datetime.now(), #TODO!! we have to take the local instance time
                                description="The datetime we want to publish the disruptions from."
                                            " Default is the current date and it is mainly used for debug."
                                            " Note: it is not the same as 'datetime' which, combined with"
                                            " duration form a period used to filter the disruption"
                                            " application periods")
        parser_get.add_argument("forbidden_id[]", type=unicode,
                                description="forbidden ids",
                                dest="forbidden_uris[]",
                                action="append")

    @marshal_with(disruptions)
    @ManageError()
    def get(self, region=None, lon=None, lat=None, uri=None):
        self.region = i_manager.get_region(region, lon, lat)
        timezone.set_request_timezone(self.region)
        args = self.parsers["get"].parse_args()

        if uri:
            if uri[-1] == "/":
                uri = uri[:-1]
            uris = uri.split("/")
            args["filter"] = self.get_filter(uris)
        else:
            args["filter"] = ""

        #we compute the period end with the duration (or the deprecated 'period' argument)
        period_end = args['datetime'] + args['duration']
        if args.get('period'):
            period_end = args['datetime'] + timedelta(days=args.get('period'))

        args['period_end'] = period_end

        if args['period_end'] < args['datetime']:
            if args.get('period'):
                abort(400, message="The 'period' parameter cannot be negative")
            else:
                abort(400, message="The 'duration' parameter cannot be negative")

        response = i_manager.dispatch(args, "disruptions",
                                      instance_name=self.region)
        return response
