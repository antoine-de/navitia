#  Copyright (c) 2001-2016, Canal TP and/or its affiliates. All rights reserved.
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

from __future__ import absolute_import, print_function, unicode_literals, division

from jormungandr.interfaces.v1.serializer import jsonschema
import serpy
import operator


class PbField(jsonschema.Field):
    """
    This field handle protobuf, it aim to handle field absent value:
    When a field is not present protobuf return a default object, but we want a None.
    If the object is always initialised it's recommended to use serpy.Field as it will be faster
    """
    def as_getter(self, serializer_field_name, serializer_cls):
        op = operator.attrgetter(self.attr or serializer_field_name)
        def getter(obj):
            try:
                if obj.HasField(self.attr or serializer_field_name):
                    return op(obj)
                else:
                    return None
            except ValueError:
                #HasField throw an exception if the field is repeated...
                return op(obj)
        return getter


class PbNestedSerializer(serpy.Serializer, PbField):
    pass


class EnumField(jsonschema.Field):


    def as_getter(self, serializer_field_name, serializer_cls):
        #For enum we need the full object :(
        return lambda x: x

    def to_value(self, value):
        if not value.HasField(self.attr):
            return None
        enum = value.DESCRIPTOR.fields_by_name[self.attr].enum_type.values_by_number
        return enum[getattr(value, self.attr)].name.lower()

    """
    Need to find a way to read protobuf
    """
    def __init__(self, schema_metadata={}, **kwargs):
        schema_type = str
        enum = []
        attr = kwargs.get('attr')
        if attr == 'id':
            enum.extend((
                'bad_filter', 'unknown_api', 'date_out_of_bounds', 'unable_to_parse', 'bad_format', 'no_origin',
                'no_destination', 'no_origin_nor_destination', 'no_solution', 'unknown_object', 'service_unavailable',
                'invalid_protobuf_request', 'internal_error'
            ))
        elif attr == 'embedded_type':
            enum.extend((
                'line', 'journey_pattern', 'vehicle_journey', 'stop_point', 'stop_area', 'network', 'physical_mode',
                'commercial_mode', 'connection', 'journey_pattern_point', 'company', 'route', 'poi', 'contributor',
                'address', 'poitype', 'administrative_region', 'calendar', 'line_group', 'impact', 'dataset', 'trip'
            ))
        elif attr == 'effect':
            enum.extend(('delayed', 'added', 'deleted'))
        elif attr == 'status':
            enum.extend(('past', 'active', 'future'))

        if len(enum) > 0:
            schema_metadata.update(enum=enum)

        super(EnumField, self).__init__(schema_type, schema_metadata, **kwargs)

class EnumListField(EnumField):
    def to_value(self, obj):
        enum = obj.DESCRIPTOR.fields_by_name[self.attr].enum_type.values_by_number
        return [enum[value].name.lower() for value in getattr(obj, self.attr)]


class GenericSerializer(PbNestedSerializer):
    id = jsonschema.Field(schema_type=str, attr='uri')
    name = jsonschema.Field(schema_type=str)


class LiteralField(jsonschema.Field):
    """
    :return literal value
    """
    def __init__(self, value, *args, **kwargs):
        super(LiteralField, self).__init__(*args, **kwargs)
        self.value = value

    def as_getter(self, serializer_field_name, serializer_cls):
        return lambda *args, **kwargs: self.value


def flatten(obj):
    if type(obj) != dict:
        raise ValueError("Invalid argument")
    new_obj = {}
    for key, value in obj.items():
        if type(value) == dict:
            tmp = {'.'.join([key, _key]): _value for _key, _value in flatten(value).items()}
            new_obj.update(tmp)
        else:
            new_obj[key] = value
    return new_obj


def value_by_path(obj, path, default=None):
    new_obj = flatten(obj)
    if new_obj:
        return new_obj.get(path, default)
    else:
        return default


class NestedPropertyField(jsonschema.Field):
    def as_getter(self, serializer_field_name, serializer_cls):
        return lambda v: value_by_path(v, self.attr)


class IntNestedPropertyField(NestedPropertyField):
    to_value = staticmethod(int)


class LambdaField(serpy.Field):
    getter_takes_serializer = True

    def __init__(self, method, **kwargs):
        super(LambdaField, self).__init__(**kwargs)
        self.method = method

    def as_getter(self, serializer_field_name, serializer_cls):
        return self.method
