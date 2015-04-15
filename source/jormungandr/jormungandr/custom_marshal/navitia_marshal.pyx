#coding=utf-8

#  Copyright (c) 2001-2015, Canal TP and/or its affiliates. All rights reserved.
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
from collections import OrderedDict
from functools import wraps
from flask.ext.restful.utils import unpack
import datetime
import pytz

class marshal_info(object):
    def __init__(self, timezone):
        self.timezone = timezone

class custom_marshal_with:
    def __init__(self, fields, timezone):
        self.fields = fields
        self.marshal_info = marshal_info(timezone)

    def __call__(self, f):

        @wraps(f)
        def wrapper(*args, **kwargs):
            resp = f(*args, **kwargs)
            if isinstance(resp, tuple):
                data, code, headers = unpack(resp)
                return simple_marshal(data, self.fields, self.marshal_info), code, headers
            else:
                return simple_marshal(resp, self.fields, self.marshal_info)
        return wrapper

def simple_marshal(data, fields, marshal_info, display_null=False):
    items = OrderedDict()
    for k, v in fields.items():
        tmp = v.output(k, data, marshal_info)
        if tmp is not None or display_null:
            items[k] = tmp
    return items


def default_getter(obj, attribute):
    if not obj.HasField(attribute):
        return None
    return getattr(obj, attribute)


def list_getter(obj, attribute):
    return getattr(obj, attribute)


class simple_atttribute_getter(object):
    def __init__(self, attribute):
        self.attribute = attribute

    def __call__(self, obj, attribute):
        return default_getter(obj, self.attribute)


class label_getter(object):
    def __call__(self, obj, key):
        return obj.code if obj.code != '' else obj.name


class Raw(object):
    def __init__(self, getter=default_getter):
        self.getter = getter

    def format(self, value, marshal_info):
        return value

    def output(self, key, obj, marshal_info):
        value = self.getter(obj, key)
        if not value:
            return None
        return self.format(value, marshal_info)


class Integer(Raw):
    def __init__(self, **kwargs):
        super(Integer, self).__init__(**kwargs)

    def format(self, value, marshal_info):
        return int(value)


class Float(Raw):
    def format(self, value, marshal_info):
        return repr(float(value))


class Nested(Raw):
    def __init__(self, nested_field, display_null=False, **kwargs):
        self.nested_field = nested_field
        self.display_null = display_null
        super(Nested, self).__init__(**kwargs)

    def output(self, key, obj, marshal_info):
        value = self.getter(obj, key)
        if not value:
            return None
        return simple_marshal(value, self.nested_field, marshal_info, self.display_null)


class List(Raw):
    def __init__(self, nested_field, getter=list_getter, display_empty=False, display_null=False, **kwargs):
        super(List, self).__init__(getter, **kwargs)
        self.display_empty = display_empty
        self.display_null = display_null
        self.nested_field = nested_field

    def output(self, key, obj, marshal_info):
        values = self.getter(obj, key)

        if not values and not self.display_empty:
            return None

        return [simple_marshal(value, self.nested_field, marshal_info, self.display_null) for value in values]


class enum_type(Raw):
    def output(self, key, obj, marshal_info):
        value = self.getter(obj, key)

        enum = obj.DESCRIPTOR.fields_by_name[key].enum_type.values_by_number
        return str.lower(enum[value].name)


class DateTime(Raw):
    """
    custom date format from timestamp
    """
    def format(self, value, marshal_info):
        #TODO, try to find faster way to do that
        dt = datetime.datetime.utcfromtimestamp(value)

        if marshal_info.timezone:
            dt = pytz.utc.localize(dt)
            dt = dt.astimezone(marshal_info.timezone)
            return dt.strftime("%Y%m%dT%H%M%S")
        return None  # for the moment I prefer not to display anything instead of something wrong


__date_time_null_value__ = 2**64 - 1


class SplitDateTime(DateTime):
    def __init__(self, date, time, *args, **kwargs):
        super(DateTime, self).__init__(*args, **kwargs)
        self.date = date
        self.time = time

    def output(self, key, obj, marshal_info):
        date = self.getter(obj, self.date) if self.date else None
        time = self.getter(obj, self.time)

        if time == __date_time_null_value__:
            return ""

        if not date:
            return self.format_time(time)

        date_time = date + time
        return self.format(date_time, marshal_info)

    @staticmethod
    def format_time(time):
        t = datetime.datetime.utcfromtimestamp(time)
        return t.strftime("%H%M%S")


"""
=============================================================
split in different files after
"""
from copy import deepcopy
# from jormungandr import utils
# from jormungandr.interfaces.v1.make_links import create_internal_link
from navitiacommon import type_pb2

class DisruptionsField(Raw):
    """
    Dump the real disruptions (and there will be link to them)
    """
    def output(self, key, obj, marshal_info):
        all_disruptions = {}

        def get_all_disruptions(_, val):
            if not hasattr(val, 'disruptions'):
                return
            disruptions = val.disruptions
            if not disruptions or not hasattr(disruptions[0], 'uri'):
                return

            for d in disruptions:
                all_disruptions[d.uri] = d

        # utils.walk_protobuf(obj, get_all_disruptions)

        return [simple_marshal(d, disruption_marshaller, marshal_info, display_null=False) for d in all_disruptions.values()]


class DisruptionLinks(Raw):
    """
    Add link to disruptions on a pt object
    """
    def output(self, key, obj, marshal_info):
        return []
        # return [create_internal_link(_type="disruption", rel="disruptions", id=d.uri)
        #         for d in obj.disruptions]


class stop_time_properties_links(Raw):

    def output(self, key, obj, marshal_info):
        properties = obj.properties
        r = []
        for note_ in properties.notes:
            r.append({"id": note_.uri, "type": "notes", "value": note_.note,
                      "internal": True})
        for exception in properties.exceptions:
            r.append({"type": "exceptions", "id": exception.uri, "date": exception.date,
                      "except_type": exception.type})
        if properties.destination and properties.destination.uri:
            r.append({"type": "notes", "id": properties.destination.uri,
                      "value": properties.destination.destination})
        if properties.vehicle_journey_id:
            r.append({"type": "vehicle_journey", "rel": "vehicle_journey",
                      "value": properties.vehicle_journey_id})
        return r

disruption_marshaller = {
    "id": Raw(getter=simple_atttribute_getter(attribute='uri')),
    # "disruption_id": Raw(getter=simple_atttribute_getter(attribute='disruption_uri')),
    # "impact_id": Raw(getter=simple_atttribute_getter(attribute='uri')),
    # "title": Raw(),
    # "application_periods": List(period),
    # "status": disruption_status,
    # "updated_at": DateTime(),
    # # "tags": List(Raw()),
    # "cause": Raw(),
    # "severity": Nested(disruption_severity),
    # "messages": List(disruption_message),
    # "uri": Raw(),
    # "disruption_uri": Raw(),
}


period = {
    "begin": DateTime(),
    "end": DateTime(),
}

coord = {
    "lon": Float(),
    "lat": Float()
}

generic_type = {
    "name": Raw(),
    "id": Raw(getter=simple_atttribute_getter(attribute="uri")),
    "coord": Nested(coord, True)
}

code = {
    "type": Raw(),
    "value": Raw()
}
admin = deepcopy(generic_type)
admin["level"] = Integer
admin["zip_code"] = Raw()
admin["label"] = Raw()
admin["insee"] = Raw()

generic_type_admin = deepcopy(generic_type)
admins = List(admin)
generic_type_admin["administrative_regions"] = admins

stop_point = deepcopy(generic_type_admin)
# stop_point["links"] = DisruptionLinks()
stop_point["comment"] = Raw()
stop_point["codes"] = List(code)
stop_point["label"] = Raw()
stop_area = deepcopy(generic_type_admin)
# stop_area["links"] = DisruptionLinks()
stop_area["comment"] = Raw()
stop_area["codes"] = List(code)
stop_area["timezone"] = Raw()
stop_area["label"] = Raw()

pagination = {
    "total_result": Integer(getter=simple_atttribute_getter(attribute="totalResult")),
    "start_page": Integer(getter=simple_atttribute_getter(attribute="startPage")),
    "items_per_page": Integer(getter=simple_atttribute_getter(attribute="itemsPerPage")),
    "items_on_page": Integer(getter=simple_atttribute_getter(attribute="itemsOnPage")),
}


class additional_informations(Raw):
    def output(self, key, obj, marshal_info):
        return [str.lower(type_pb2.Properties.AdditionalInformation.Name(v)) for v
                in obj.properties.additional_informations]

date_time = {
    "date_time": SplitDateTime(date='date', time='time'),
    "additional_informations": additional_informations(),
    "links": stop_time_properties_links()
}

error = {
    'id': enum_type(),
    'message': Raw()
}

row = {
    "stop_point": Nested(stop_point),
    "date_times": List(date_time)
}

display_informations_vj = {
    "description": Raw(),
    "physical_mode": Raw(),
    "commercial_mode": Raw(),
    "network": Raw(),
    "direction": Raw(),
    "label": Raw(label_getter()),
    "color": Raw(),
    "code": Raw(),
    # "equipments": equipments(attribute="has_equipments"),
    "headsign": Raw(),
    "links": DisruptionLinks(),
}


class additional_informations_vj(Raw):

    def output(self, key, obj, marshal_info):
        addinfo = obj.add_info_vehicle_journey
        result = []
        if addinfo.has_date_time_estimated:
            result.append("has_date_time_estimated")

        if addinfo.stay_in:
            result.append('stay_in')

        vj_type = addinfo.vehicle_journey_type
        if vj_type == type_pb2.virtual_with_stop_time:
            result.append("odt_with_stop_time")
        elif vj_type in [type_pb2.virtual_without_stop_time,
                         type_pb2.stop_point_to_stop_point,
                         type_pb2.address_to_stop_point,
                         type_pb2.odt_point_to_point]:
                result.append("odt_with_zone")
        else:
            result.append("regular")
        return result


class UrisToLinks(Raw):

    def output(self, key, obj, marshal_info):
        display_info = obj.pt_display_informations
        uris = display_info.uris
        response = []
        if uris.line != '':
            response.append({"type": "line", "id": uris.line})
        if uris.company != '':
            response.append({"type": "company", "id": uris.company})
        if uris.vehicle_journey != '':
            response.append({"type": "vehicle_journey",
                             "id": uris.vehicle_journey})
        if uris.route != '':
            response.append({"type": "route", "id": uris.route})
        if uris.commercial_mode != '':
            response.append({"type": "commercial_mode",
                             "id": uris.commercial_mode})
        if uris.physical_mode != '':
            response.append({"type": "physical_mode",
                             "id": uris.physical_mode})
        if uris.network != '':
            response.append({"type": "network", "id": uris.network})
        if uris.note != '':
            response.append({"type": "note", "id": uris.note})

        for value in display_info.notes:
            response.append({"type": 'notes',
                             "id": value.uri,
                             'value': value.note,
                             'internal': True})
        return response


header = {
    "display_informations": Nested(display_informations_vj,
                                   getter=simple_atttribute_getter(attribute='pt_display_informations')),
    "additional_informations": additional_informations_vj(),
    "links": UrisToLinks()
}
table_field = {
    "rows": List(row),
    "headers": List(header)
}


class MultiLineString(Raw):
    def format(self, val, marshal_info):
        lines = []
        for l in val.lines:
            lines.append([[c.lon, c.lat] for c in l.coordinates])

        response = {
            "type": "MultiLineString",
            "coordinates": lines,
        }
        return response


class RouteSchedulesLinkField(Raw):

    def output(self, key, obj, marshal_info):
        if obj.HasField('pt_display_informations'):
            return [{"type": 'notes', "id": value.uri, 'value': value.note}
                    for value in obj.pt_display_informations.notes]


display_informations_route = {
    "network": Raw(),
    "direction": Raw(),
    "commercial_mode": Raw(),
    "label": Raw(label_getter()),
    "color": Raw(),
    "code": Raw(),
    "links": DisruptionLinks(),
}

route_schedule_fields = {
    "table": Nested(table_field),
    "display_informations": Nested(display_informations_route,
                                   getter=simple_atttribute_getter(attribute='pt_display_informations')),
    "links": RouteSchedulesLinkField(),
    "geojson": MultiLineString()
}

route_schedules = {
    "error": Nested(error),
    "route_schedules": List(route_schedule_fields),
    "pagination": Nested(pagination),
    "disruptions": DisruptionsField(),
}
