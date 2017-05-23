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

import serpy


class Field(serpy.Field):
    """
    A :class:`Field` that hold metadata for schema.
    """
    def __init__(self, schema_type=None, schema_metadata={}, **kwargs):
        if 'display_none' not in kwargs:
            kwargs['display_none'] = False
        super(Field, self).__init__(**kwargs)
        self.schema_type = schema_type or str
        self.schema_metadata = schema_metadata


class StrField(serpy.StrField):
    """
    A :class:`StrField` that hold metadata for schema.
    """

    def __init__(self, schema_metadata={}, **kwargs):
        if 'display_none' not in kwargs:
            kwargs['display_none'] = False
        super(StrField, self).__init__(**kwargs)
        self.schema_type = str
        self.schema_metadata = schema_metadata


class BoolField(serpy.BoolField):
    """
    A :class:`BoolField` that hold metadata for schema.
    """

    def __init__(self, schema_metadata={}, **kwargs):
        if 'display_none' not in kwargs:
            kwargs['display_none'] = False
        super(BoolField, self).__init__(**kwargs)
        self.schema_type = bool
        self.schema_metadata = schema_metadata


class FloatField(serpy.FloatField):
    """
    A :class:`FloatField` that hold metadata for schema.
    """

    def __init__(self, schema_metadata={}, **kwargs):
        if 'display_none' not in kwargs:
            kwargs['display_none'] = False
        super(FloatField, self).__init__(**kwargs)
        self.schema_type = float
        self.schema_metadata = schema_metadata


class IntField(serpy.IntField):
    """
    A :class:`IntField` that hold metadata for schema.
    """

    def __init__(self, schema_metadata={}, **kwargs):
        if 'display_none' not in kwargs:
            kwargs['display_none'] = False
        super(IntField, self).__init__(**kwargs)
        self.schema_type = int
        self.schema_metadata = schema_metadata


class MethodField(serpy.MethodField):
    """
    A :class:`MethodField` that hold metadata for schema.
    """

    def __init__(self, method=None, schema_type=None, schema_metadata={}, **kwargs):
        if 'display_none' not in kwargs:
            kwargs['display_none'] = False
        super(MethodField, self).__init__(method, **kwargs)
        self.schema_type = schema_type
        self.schema_metadata = schema_metadata
