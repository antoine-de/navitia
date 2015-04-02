# coding=utf-8

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

from flask.ext.restful import fields, marshal_with, reqparse
from flask.globals import g
from flask import request
from jormungandr import i_manager, utils
from jormungandr import timezone
from fields import stop_point, route, pagination, PbField, stop_date_time, \
    additional_informations, stop_time_properties_links, display_informations_vj, \
    display_informations_route, additional_informations_vj, UrisToLinks, error, \
    enum_type, SplitDateTime, MultiLineString
from ResourceUri import ResourceUri, complete_links
import datetime
from jormungandr.interfaces.argument import ArgumentDoc
from jormungandr.interfaces.parsers import option_value, date_time_format
from errors import ManageError
from flask.ext.restful.types import natural, boolean
from jormungandr.interfaces.v1.fields import DisruptionsField
from jormungandr.utils import ResourceUtc
from make_links import create_external_link
from functools import wraps
from copy import deepcopy


class RouteSchedulesLinkField(fields.Raw):

    def output(self, key, obj):
        response = []
        if obj.HasField('pt_display_informations'):
            for value in obj.pt_display_informations.notes:
                response.append({"type": 'notes', "id": value.uri, 'value': value.note})
        return response

class Schedules(ResourceUri, ResourceUtc):
    parsers = {}

    def __init__(self, endpoint):
        ResourceUri.__init__(self)
        ResourceUtc.__init__(self)
        self.endpoint = endpoint
        self.parsers["get"] = reqparse.RequestParser(
            argument_class=ArgumentDoc)
        parser_get = self.parsers["get"]
        parser_get.add_argument("filter", type=str)
        parser_get.add_argument("from_datetime", type=date_time_format,
                                description="The datetime from which you want\
                                the schedules", default=None)
        parser_get.add_argument("until_datetime", type=date_time_format,
                                description="The datetime until which you want\
                                the schedules", default=None)
        parser_get.add_argument("duration", type=int, default=3600 * 24,
                                description="Maximum duration between datetime\
                                and the retrieved stop time")
        parser_get.add_argument("depth", type=int, default=2)
        parser_get.add_argument("count", type=int, default=10,
                                description="Number of schedules per page")
        parser_get.add_argument("start_page", type=int, default=0,
                                description="The current page")
        parser_get.add_argument("max_date_times", type=natural, default=10000,
                                description="Maximum number of schedule per\
                                stop_point/route")
        parser_get.add_argument("forbidden_id[]", type=unicode,
                                description="forbidden ids",
                                dest="forbidden_uris[]",
                                action="append")
        parser_get.add_argument("calendar", type=str,
                                description="Id of the calendar")
        parser_get.add_argument("show_codes", type=boolean, default=False,
                            description="show more identification codes")
        parser_get.add_argument("_current_datetime", type=date_time_format, default=datetime.datetime.utcnow(),
                                description="The datetime we want to publish the disruptions from."
                                            " Default is the current date and it is mainly used for debug.")

        self.method_decorators.append(complete_links(self))

    def get(self, uri=None, region=None, lon=None, lat=None):
        args = self.parsers["get"].parse_args()
        args["nb_stoptimes"] = args["count"]
        args["interface_version"] = 1

        if uri is None:
            first_filter = args["filter"].lower().split("and")[0].strip()
            parts = first_filter.lower().split("=")
            if len(parts) != 2:
                error = "Unable to parse filter {filter}"
                return {"error": error.format(filter=args["filter"])}, 503
            else:
                self.region = i_manager.get_region(object_id=parts[1].strip())
        else:
            self.collection = 'schedules'
            args["filter"] = self.get_filter(uri.split("/"))
            self.region = i_manager.get_region(region, lon, lat)
        timezone.set_request_timezone(self.region)

        if not args["from_datetime"] and not args["until_datetime"]:
            args['from_datetime'] = datetime.datetime.now()
            args['from_datetime'] = args['from_datetime'].replace(hour=13, minute=37)

        # we save the original datetime for debuging purpose
        args['original_datetime'] = args['from_datetime']

        if not args.get('calendar'):
            #if a calendar is given all times will be given in local (because it might span over dst)
            if args['from_datetime']:
                new_datetime = self.convert_to_utc(args['from_datetime'])
                args['from_datetime'] = utils.date_to_timestamp(new_datetime)
            if args['until_datetime']:
                new_datetime = self.convert_to_utc(args['until_datetime'])
                args['until_datetime'] = utils.date_to_timestamp(new_datetime)
        else:
            args['from_datetime'] = utils.date_to_timestamp(args['from_datetime'])

        if not args["from_datetime"] and args["until_datetime"]\
                and self.endpoint[:4] == "next":
            self.endpoint = "previous" + self.endpoint[4:]

        self._register_interpreted_parameters(args)
        return i_manager.dispatch(args, self.endpoint,
                                  instance_name=self.region)


date_time = {
    "date_time": SplitDateTime(date='date', time='time'),
    "additional_informations": additional_informations(),
    "links": stop_time_properties_links()
}

row = {
    "stop_point": PbField(stop_point),
    "date_times": fields.List(fields.Nested(date_time))
}

header = {
    "display_informations": PbField(display_informations_vj,
                                    attribute='pt_display_informations'),
    "additional_informations": additional_informations_vj(),
    "links": UrisToLinks()
}
table_field = {
    "rows": fields.List(fields.Nested(row)),
    "headers": fields.List(fields.Nested(header))
}

route_schedule_fields = {
    "table": PbField(table_field),
    "display_informations": PbField(display_informations_route,
                                    attribute='pt_display_informations'),
    "links": RouteSchedulesLinkField(attribute='pt_display_informations'),
    "geojson": MultiLineString()
}

route_schedules = {
    "error": PbField(error, attribute='error'),
    "route_schedules": fields.List(fields.Nested(route_schedule_fields)),
    "pagination": fields.Nested(pagination),
    "disruptions": DisruptionsField,
}


class RouteSchedules(Schedules):

    def __init__(self):
        super(RouteSchedules, self).__init__("route_schedules")

    @marshal_with(route_schedules)
    @ManageError()
    def get(self, uri=None, region=None, lon=None, lat=None):
        return super(RouteSchedules, self).get(uri=uri, region=region, lon=lon,
                                               lat=lat)


stop_schedule = {
    "stop_point": PbField(stop_point),
    "route": PbField(route, attribute="route"),
    "additional_informations": enum_type(attribute="response_status"),
    "display_informations": PbField(display_informations_route,
                                    attribute='pt_display_informations'),
    "date_times": fields.List(fields.Nested(date_time)),
    "links": UrisToLinks()
}
stop_schedules = {
    "stop_schedules": fields.List(fields.Nested(stop_schedule)),
    "pagination": fields.Nested(pagination),
    "error": PbField(error, attribute='error'),
    "disruptions": DisruptionsField,
}


class StopSchedules(Schedules):

    def __init__(self):
        super(StopSchedules, self).__init__("departure_boards")
        self.parsers["get"].add_argument("interface_version", type=int,
                                         default=1, hidden=True)

    @marshal_with(stop_schedules)
    @ManageError()
    def get(self, uri=None, region=None, lon=None, lat=None):
        return super(StopSchedules, self).get(uri=uri, region=region, lon=lon,
                                              lat=lat)


passage = {
    "route": PbField(route, attribute="vehicle_journey.route"),
    "stop_point": PbField(stop_point),
    "stop_date_time": PbField(stop_date_time)
}

departures = {
    "departures": fields.List(fields.Nested(passage),
                              attribute="next_departures"),
    "pagination": fields.Nested(pagination),
    "error": PbField(error, attribute='error'),
    "disruptions": DisruptionsField,
}

arrivals = {
    "arrivals": fields.List(fields.Nested(passage), attribute="next_arrivals"),
    "pagination": fields.Nested(pagination),
    "error": PbField(error, attribute='error'),
    "disruptions": DisruptionsField,
}

class add_passages_links:
    """
    delete disruption links and put the disruptions directly in the owner objets

    TEMPORARY: delete this as soon as the front end has the new disruptions integrated
    """
    def __call__(self, f):
        @wraps(f)
        def wrapper(*args, **kwargs):
            response, status, other = f(*args, **kwargs)
            api = "departures" if "departures" in response else "arrivals" if "arrivals" in response else None
            if not api:
                return response, status, other
            passages = response[api]

            max_dt = "19000101T000000"
            min_dt = "29991231T235959"
            time_field = "arrival_date_time" if api == "arrivals" else "departure_date_time"
            for passage_ in passages:
                dt = passage_["stop_date_time"][time_field]
                if min_dt > dt:
                    min_dt = dt
                if max_dt < dt:
                    max_dt = dt
            if "links" not in response:
                response["links"] = []
            kwargs_links = dict(deepcopy(request.args))
            if "region" in kwargs:
                kwargs_links["region"] = kwargs["region"]
            if "uri" in kwargs:
                kwargs_links["uri"] = kwargs["uri"]
            if 'from_datetime' in kwargs_links:
                kwargs_links.pop('from_datetime')
            delta = datetime.timedelta(seconds=1)
            dt = datetime.datetime.strptime(min_dt, "%Y%m%dT%H%M%S")
            kwargs_links['until_datetime'] = (dt - delta).strftime("%Y%m%dT%H%M%S")
            response["links"].append(create_external_link("v1."+api, rel="prev", _type=api, **kwargs_links))
            kwargs_links.pop('until_datetime')
            kwargs_links['from_datetime'] = (datetime.datetime.strptime(max_dt, "%Y%m%dT%H%M%S") + delta).strftime("%Y%m%dT%H%M%S")
            response["links"].append(create_external_link("v1."+api, rel="next", _type=api, **kwargs_links))
            return response, status, other
        return wrapper


class NextDepartures(Schedules):

    def __init__(self):
        super(NextDepartures, self).__init__("next_departures")

    @add_passages_links()
    @marshal_with(departures)
    @ManageError()
    def get(self, uri=None, region=None, lon=None, lat=None,
            dest="nb_stoptimes"):
        return super(NextDepartures, self).get(uri=uri, region=region, lon=lon,
                                               lat=lat)


class NextArrivals(Schedules):

    def __init__(self):
        super(NextArrivals, self).__init__("next_arrivals")

    @add_passages_links()
    @marshal_with(arrivals)
    @ManageError()
    def get(self, uri=None, region=None, lon=None, lat=None):
        return super(NextArrivals, self).get(uri=uri, region=region, lon=lon,
                                             lat=lat)
