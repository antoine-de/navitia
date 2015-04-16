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
import logging
from flask.ext.restful.utils import unpack
import datetime
import pytz

#import to be split in navitia_fields
# from jormungandr import utils
# from jormungandr.interfaces.v1.make_links import create_internal_link
from navitiacommon import type_pb2

cdef class MarshalInfo(object):
    cdef public timezone
    def __init__(self, timezone):
        self.timezone = timezone

class custom_marshal_with:
    def __init__(self, fields, timezone):
        self.fields = fields
        self.misc_info = MarshalInfo(timezone)

    def __call__(self, f):

        @wraps(f)
        def wrapper(*args, **kwargs):
            resp = f(*args, **kwargs)
            if isinstance(resp, tuple):
                data, code, headers = unpack(resp)
                return simple_marshal(data, self.fields, self.misc_info), code, headers
            else:
                return simple_marshal(resp, self.fields, self.misc_info)
        return wrapper

cpdef simple_marshal(data, fields, misc_info, display_null=False):
    cdef items = OrderedDict()
    cdef basestring k
    cdef Raw v

    for k, v in fields.items():

        # logging.error(" ++ key = {}, value = {}".format(k, v))
        tmp = v.output(k, data, misc_info)
        # logging.error("res = {}".format(tmp))
        if tmp is not None or display_null:
            items[k] = tmp
    return items

cdef class Getter:
    cdef get(self, obj, basestring attribute):
        raise NotImplementedError

cdef class default_getter(Getter):
    cdef get(self, obj, basestring attribute):
        # logging.warn('getter for {} and {}'.format(obj, attribute))
        if not obj.HasField(attribute):
            return None
        attr = getattr(obj, attribute)

        # if type(attr) in [bytes, str, unicode, basestring]:
        #     logging.warn("obob???? {}".format(type(attr)))
        #     attr = <bytes>attr
        # logging.warn('getter returned value: {}'.format(attr))
        if attr is not None and attr != '':
            return attr
        return None


cdef class list_getter(Getter):
    cdef get(self, obj, basestring attribute):
        return getattr(obj, attribute)


cdef class simple_atttribute_getter(Getter):
    cdef basestring attribute
    def __init__(self, basestring attribute):
        self.attribute = attribute

    cdef get(self, obj, basestring attribute):
        return default_getter().get(obj, self.attribute)


cdef class label_getter(Getter):
    cdef get(self, obj, basestring attribute):
        return obj.code if obj.code != '' else obj.name


cdef class Raw(object):
    cdef readonly Getter getter
    def __init__(self, Getter getter=default_getter()):
        self.getter = getter
        if self.getter is None:
            raise Exception('mierda')

    cdef format(self, value, misc_info):
        return value

    cdef output(self, key, obj, MarshalInfo misc_info):

        # logging.error("---------------")
        # logging.error("getter = {}, obj = {}".format(self.getter, self.__class__))
        value = self.getter.get(obj, key)
        # logging.error("=============================================")
        # logging.error("value = {}".format(value))
        # logging.error("=============================================")
        if not value:
            return None
        return self.format(value, misc_info)


cdef class Integer(Raw):
    cdef format(self, value, misc_info):
        return int(value)


cdef class Float(Raw):
    cdef format(self, value, misc_info):
        return repr(float(value))


cdef class Nested(Raw):
    cdef nested_field
    cdef bint display_null

    def __init__(self, nested_field, display_null=False, **kwargs):
        self.nested_field = nested_field
        self.display_null = display_null
        super(Nested, self).__init__(**kwargs)

    cdef output(self, key, obj, MarshalInfo misc_info):
        value = self.getter.get(obj, key)
        if not value:
            return None
        return simple_marshal(value, self.nested_field, misc_info, self.display_null)


cdef class List(Raw):
    cdef bint display_empty
    cdef bint display_null
    cdef nested_field

    def __init__(self, nested_field, Getter getter=list_getter(), display_empty=False, display_null=False, **kwargs):
        super(List, self).__init__(getter, **kwargs)
        self.display_empty = display_empty
        self.display_null = display_null
        self.nested_field = nested_field

    cdef output(self, key, obj, MarshalInfo misc_info):
        values = self.getter.get(obj, key)

        if not values and not self.display_empty:
            return None

        return [simple_marshal(value, self.nested_field, misc_info, self.display_null) for value in values]


cdef class enum_type(Raw):
    cdef output(self, key, obj, MarshalInfo misc_info):
        value = self.getter.get(obj, key)

        enum = obj.DESCRIPTOR.fields_by_name[key].enum_type.values_by_number
        return str.lower(enum[value].name)


cdef class DateTime(Raw):
    """
    custom date format from timestamp
    """
    cdef format(self, value, misc_info):
        #TODO, try to find faster way to do that
        dt = datetime.datetime.utcfromtimestamp(value)

        if misc_info.timezone:
            dt = pytz.utc.localize(dt)
            dt = dt.astimezone(misc_info.timezone)
            return dt.strftime("%Y%m%dT%H%M%S")
        return None  # for the moment I prefer not to display anything instead of something wrong


__date_time_null_value__ = 2**64 - 1


cdef class SplitDateTime(DateTime):
    cdef basestring date
    cdef basestring time

    def __init__(self, date, time, *args, **kwargs):
        super(SplitDateTime, self).__init__(*args, **kwargs)
        if self.getter is None:
            raise Exception("miiiiiiiiiiiiiiiiiiierda")
        self.date = date
        self.time = time

    cdef output(self, key, obj, MarshalInfo misc_info):
        date = self.getter.get(obj, self.date) if self.date else None
        time = self.getter.get(obj, self.time)

        if time == __date_time_null_value__:
            return ""

        if not date:
            return self.format_time(time)

        date_time = date + time
        return self.format(date_time, misc_info)

    cdef format_time(self, time):
        t = datetime.datetime.utcfromtimestamp(time)
        return t.strftime("%H%M%S")


"""
=============================================================
split in different files after
"""

cdef class DisruptionsField(Raw):
    """
    Dump the real disruptions (and there will be link to them)
    """
    cdef output(self, key, obj, MarshalInfo misc_info):
        all_disruptions = {}

        # def get_all_disruptions(_, val):
        #     if not hasattr(val, 'disruptions'):
        #         return
        #     disruptions = val.disruptions
        #     if not disruptions or not hasattr(disruptions[0], 'uri'):
        #         return
        #
        #     for d in disruptions:
        #         all_disruptions[d.uri] = d

        # utils.walk_protobuf(obj, get_all_disruptions)

        return []
        # return [simple_marshal(d, disruption_marshaller, misc_info, display_null=False) for d in all_disruptions.values()]


cdef class DisruptionLinks(Raw):
    """
    Add link to disruptions on a pt object
    """
    cdef output(self, key, obj, MarshalInfo misc_info):
        return []
        # return [create_internal_link(_type="disruption", rel="disruptions", id=d.uri)
        #         for d in obj.disruptions]


cdef class stop_time_properties_links(Raw):

    cdef output(self, key, obj, MarshalInfo misc_info):
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
    # "name": Raw(),
    "id": Raw(getter=simple_atttribute_getter(attribute="uri")),
    "coord": Nested(coord, True)
}

code = {
    "type": Raw(),
    "value": Raw()
}
admin = {
    # "name": Raw(),
    "id": Raw(getter=simple_atttribute_getter(attribute="uri")),
    "coord": Nested(coord, True)
}
admin["level"] = Integer
admin["zip_code"] = Raw()
# admin["label"] = Raw()
admin["insee"] = Raw()

generic_type_admin = {
    # "name": Raw(),
    "id": Raw(getter=simple_atttribute_getter(attribute="uri")),
    "coord": Nested(coord, True)
}
admins = List(admin)
generic_type_admin["administrative_regions"] = admins

stop_point = {
    # "name": Raw(),
    "id": Raw(getter=simple_atttribute_getter(attribute="uri")),
    "coord": Nested(coord, True)
}
# stop_point["links"] = DisruptionLinks()
stop_point["comment"] = Raw()
stop_point["codes"] = List(code)
# stop_point["label"] = Raw()
stop_area = {
    # "name": Raw(),
    "id": Raw(getter=simple_atttribute_getter(attribute="uri")),
    "coord": Nested(coord, True)
}
# stop_area["links"] = DisruptionLinks()
stop_area["comment"] = Raw()
stop_area["codes"] = List(code)
stop_area["timezone"] = Raw()
# stop_area["label"] = Raw()

pagination = {
    "total_result": Integer(getter=simple_atttribute_getter(attribute="totalResult")),
    "start_page": Integer(getter=simple_atttribute_getter(attribute="startPage")),
    "items_per_page": Integer(getter=simple_atttribute_getter(attribute="itemsPerPage")),
    "items_on_page": Integer(getter=simple_atttribute_getter(attribute="itemsOnPage")),
}


cdef class additional_informations(Raw):
    cdef output(self, key, obj, MarshalInfo misc_info):
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
    "label": Raw(getter=label_getter()),
    "color": Raw(),
    "code": Raw(),
    # "equipments": equipments(attribute="has_equipments"),
    "headsign": Raw(),
    "links": DisruptionLinks(),
}


cdef class additional_informations_vj(Raw):

    cdef output(self, key, obj, MarshalInfo misc_info):
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


cdef class UrisToLinks(Raw):

    cdef output(self, key, obj, MarshalInfo misc_info):
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


cdef class MultiLineString(Raw):
    cdef format(self, val, misc_info):
        lines = []
        for l in val.lines:
            lines.append([[c.lon, c.lat] for c in l.coordinates])

        response = {
            "type": "MultiLineString",
            "coordinates": lines,
        }
        return response


cdef class RouteSchedulesLinkField(Raw):

    cdef output(self, key, obj, MarshalInfo misc_info):
        if obj.HasField('pt_display_informations'):
            return [{"type": 'notes', "id": value.uri, 'value': value.note}
                    for value in obj.pt_display_informations.notes]


display_informations_route = {
    "network": Raw(),
    "direction": Raw(),
    "commercial_mode": Raw(),
    "label": Raw(getter=label_getter()),
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
