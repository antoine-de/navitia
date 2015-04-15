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

import navitia_marshal as marshal
from jormungandr.utils import str_to_time_stamp
from navitiacommon import response_pb2, type_pb2


def test_simple_marshal():
    pagination = {
        "total_result": marshal.Integer(getter=marshal.simple_atttribute_getter(attribute="totalResult")),
        "startPage": marshal.Integer(),
    }

    r = response_pb2.Response()
    r.pagination.totalResult = 12
    r.pagination.startPage = 2
    r.pagination.itemsPerPage = 3  # not in the fields, not outputed

    res = marshal.simple_marshal(r.pagination, pagination, None)

    assert res == {'total_result': 12, 'startPage': 2}


def test_nested_marshal():

    pagination = {
        "total_result": marshal.Integer(getter=marshal.simple_atttribute_getter(attribute="totalResult")),
        "startPage": marshal.Integer(),
    }

    stop_schedules = {"pagination": marshal.Nested(pagination),}

    r = response_pb2.Response()
    r.pagination.totalResult = 12
    r.pagination.startPage = 2
    r.pagination.itemsPerPage = 3  # not in the fields, not outputed

    schedule = r.stop_schedules.add()
    schedule.response_status = type_pb2.terminus

    res = marshal.simple_marshal(r, stop_schedules, None)

    assert res == {'pagination': {'total_result': 12, 'startPage': 2}}


def test_list_marshal():

    place = {
        "id": marshal.Raw(getter=marshal.simple_atttribute_getter(attribute='uri'))
    }
    section = {
        "type": marshal.enum_type(),
    }
    journey = {
        'duration': marshal.Integer(),
        'nb_transfers': marshal.Integer(),
        'departure_date_time': marshal.Integer(),
        'sections': marshal.List(section),
        'from': marshal.Nested(place, getter=marshal.simple_atttribute_getter(attribute='origin')),
    }
    error = {
        'id': marshal.enum_type(),
        'message': marshal.Raw()
    }
    journeys = {
        "journeys": marshal.List(journey),
        "error": marshal.Nested(error),
    }
    response = response_pb2.Response()

    j1 = response.journeys.add()
    j1.type = "rapid"  # not in fields, not serialized
    j1.departure_date_time = str_to_time_stamp('20141103T123000')
    j1.duration = 42
    j1.nb_transfers = 3
    p = j1.origin
    p.uri = 'toto'
    section = j1.sections.add()
    section.type = response_pb2.STREET_NETWORK
    section = j1.sections.add()
    section.type = response_pb2.PUBLIC_TRANSPORT

    j2 = response.journeys.add()
    j2.type = "rapid"
    j2.departure_date_time = str_to_time_stamp('20141103T110000')

    res = marshal.simple_marshal(response, journeys, None)

    print res

    assert 'journeys' in res
    assert res['journeys'][0]['duration'] == 42
    assert res['journeys'][0]['nb_transfers'] == 3
    assert res['journeys'][0]['from'] == {'id': 'toto'}
    assert res['journeys'][0]['departure_date_time'] == 1415017800L
    assert res['journeys'][0]['sections'] == [{'type': 'street_network'}, {'type': 'public_transport'}]
    assert res['journeys'][1] == {'departure_date_time': 1415012400L}

def test_datetime_marshall():
    import pytz

    response = response_pb2.Response()
    pb_j = response.journeys.add()
    pb_j.departure_date_time = str_to_time_stamp('20141103T123000')

    journey = {
        'departure_date_time': marshal.DateTime(),
    }
    journeys = {
        "journeys": marshal.List(journey),
    }
    add_info = marshal.marshal_info(pytz.timezone('UTC'))
    res = marshal.simple_marshal(response, journeys, add_info)

    assert res == {'journeys': [{'departure_date_time': '20141103T123000'}]}