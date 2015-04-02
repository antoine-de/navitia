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
import logging

from navitiacommon import models
from tests_mechanism import AbstractTestFixture, dataset
from check_utils import *
from nose.tools import eq_
from overlapping_routing_tests import  MockKraken
from jormungandr import instance_manager

@dataset(["main_routing_test"])
class TestJourneys(AbstractTestFixture):
    """
    Test the structure of the journeys response
    """

    def test_journeys(self):
        #NOTE: we query /v1/coverage/main_routing_test/journeys and not directly /v1/journeys
        #not to use the jormungandr database
        response = self.query_region(journey_basic_query, display=True)

        is_valid_journey_response(response, self.tester, journey_basic_query)

    def test_error_on_journeys(self):
        """ if we got an error with kraken, an error should be returned"""

        query_out_of_production_bound = "journeys?from={from_coord}&to={to_coord}&datetime={datetime}"\
            .format(from_coord="0.0000898312;0.0000898312",  # coordinate of S in the dataset
            to_coord="0.00188646;0.00071865",  # coordinate of R in the dataset
            datetime="20110614T080000")  # 2011 should not be in the production period

        response, status = self.query_no_assert("v1/coverage/main_routing_test/" + query_out_of_production_bound)

        assert status != 200, "the response should not be valid"

        assert response['error']['id'] == "date_out_of_bounds"
        assert response['error']['message'] == "date is not in data production period"

        #and no journey is to be provided
        assert 'journeys' not in response or len(response['journeys']) == 0

    def test_best_filtering(self):
        """Filter to get the best journey, we should have only one journey, the best one"""
        query = "{query}&type=best".format(query=journey_basic_query)
        response = self.query_region(query)

        is_valid_journey_response(response, self.tester, query)
        assert len(response['journeys']) == 1

        assert response['journeys'][0]["type"] == "best"

    def test_other_filtering(self):
        """the basic query return a non pt walk journey and a best journey. we test the filtering of the non pt"""

        response = self.query_region("{query}&type=non_pt_walk".
                                     format(query=journey_basic_query))

        assert len(response['journeys']) == 1
        assert response['journeys'][0]["type"] == "non_pt_walk"

    def test_not_existent_filtering(self):
        """if we filter with a real type but not present, we don't get any journey, but we got a nice error"""

        response = self.query_region("{query}&type=car".
                                     format(query=journey_basic_query))

        assert not 'journeys' in response or len(response['journeys']) == 0
        assert 'error' in response
        assert response['error']['id'] == 'no_solution'
        assert response['error']['message'] == 'No journey found, all were filtered'

    def test_dumb_filtering(self):
        """if we filter with non existent type, we get an error"""

        response, status = self.query_region("{query}&type=sponge_bob"
                                             .format(query=journey_basic_query), check=False)

        assert status == 400, "the response should not be valid"

        assert response['message'].startswith("The type argument must be in list")

    def test_journeys_no_bss_and_walking(self):
        query = journey_basic_query + "&first_section_mode=walking&first_section_mode=bss"
        response = self.query_region(query)

        is_valid_journey_response(response, self.tester, query)
        #Note: we need to mock the kraken instances to check that only one call has been made and not 2
        #(only one for bss because walking should not have been added since it duplicate bss)

        # we explicitly check that we find both mode in the responses link
        # (is checked in is_valid_journey, but never hurts to check twice)
        links = get_links_dict(response)
        for l in ["prev", "next", "first", "last"]:
            assert l in links
            url = links[l]['href']
            url_dict = query_from_str(url)
            assert url_dict['first_section_mode'] == ['walking', 'bss']

    """
    test on date format
    """
    def test_journeys_no_date(self):
        """
        giving no date, we should have a response
        BUT, since without date we take the current date, it will not be in the production period,
        so we have a 'not un production period error'
        """

        query = "journeys?from={from_coord}&to={to_coord}"\
            .format(from_coord=s_coord, to_coord=r_coord)

        response, status_code = self.query_no_assert("v1/coverage/main_routing_test/" + query)

        assert status_code != 200, "the response should not be valid"

        assert response['error']['id'] == "date_out_of_bounds"
        assert response['error']['message'] == "date is not in data production period"

    def test_journeys_date_no_second(self):
        """giving no second in the date we should not be a problem"""

        query = "journeys?from={from_coord}&to={to_coord}&datetime={d}"\
            .format(from_coord=s_coord, to_coord=r_coord, d="20120614T0800")

        response = self.query_region(query)
        is_valid_journey_response(response, self.tester, journey_basic_query)

        #and the second should be 0 initialized
        journeys = get_not_null(response, "journeys")
        assert journeys[0]["requested_date_time"] == "20120614T080000"

    def test_journeys_date_no_minute_no_second(self):
        """giving no minutes and no second in the date we should not be a problem"""

        query = "journeys?from={from_coord}&to={to_coord}&datetime={d}"\
            .format(from_coord=s_coord, to_coord=r_coord, d="20120614T08")

        response = self.query_region(query)
        is_valid_journey_response(response, self.tester, journey_basic_query)

        #and the second should be 0 initialized
        journeys = get_not_null(response, "journeys")
        assert journeys[0]["requested_date_time"] == "20120614T080000"

    def test_journeys_date_too_long(self):
        """giving an invalid date (too long) should be a problem"""

        query = "journeys?from={from_coord}&to={to_coord}&datetime={d}"\
            .format(from_coord=s_coord, to_coord=r_coord, d="20120614T0812345")

        response, status_code = self.query_no_assert("v1/coverage/main_routing_test/" + query)

        assert not 'journeys' in response
        assert 'message' in response
        assert response['message'] == "Unable to parse datetime, unknown string format"

    def test_journeys_date_invalid(self):
        """giving the date with mmsshh (56 45 12) should be a problem"""

        query = "journeys?from={from_coord}&to={to_coord}&datetime={d}"\
            .format(from_coord=s_coord, to_coord=r_coord, d="20120614T564512")

        response, status_code = self.query_no_assert("v1/coverage/main_routing_test/" + query)

        assert not 'journeys' in response
        assert 'message' in response
        assert response['message'] == "Unable to parse datetime, hour must be in 0..23"

    def test_journeys_date_valid_not_zeropadded(self):
        """giving the date with non zero padded month should be a problem"""

        query = "journeys?from={from_coord}&to={to_coord}&datetime={d}"\
            .format(from_coord=s_coord, to_coord=r_coord, d="2012614T081025")

        response, status_code = self.query_no_assert("v1/coverage/main_routing_test/" + query)

        assert not 'journeys' in response
        assert 'message' in response
        assert response['message'] == "Unable to parse datetime, year is out of range"

    def test_journeys_do_not_loose_precision(self):
        """do we have a good precision given back in the id"""

        # this id was generated by giving an id to the api, and
        # copying it here the resulting id
        id = "8.98311981954709e-05;0.000898311281954"
        response = self.query_region("journeys?from={id}&to={id}&datetime={d}"
                                     .format(id=id, d="20120614T080000"))
        assert(len(response['journeys']) > 0)
        for j in response['journeys']:
            assert(j['sections'][0]['from']['id'] == id)
            assert(j['sections'][0]['from']['address']['id'] == id)
            assert(j['sections'][-1]['to']['id'] == id)
            assert(j['sections'][-1]['to']['address']['id'] == id)


@dataset([])
class TestJourneysNoRegion(AbstractTestFixture):
    """
    If no region loaded we must have a polite error while asking for a journey
    """

    def test_with_region(self):
        response, status = self.query_no_assert("v1/coverage/non_existent_region/" + journey_basic_query)

        assert status != 200, "the response should not be valid"

        assert response['error']['id'] == "unknown_object"
        assert response['error']['message'] == "The region non_existent_region doesn't exists"

    def test_no_region(self):
        response, status = self.query_no_assert("v1/" + journey_basic_query)

        assert status != 200, "the response should not be valid"

        assert response['error']['id'] == "unknown_object"

        error_regexp = re.compile('^No region available for the coordinates.*')
        assert error_regexp.match(response['error']['message'])


@dataset(["basic_routing_test"])
class TestLongWaitingDurationFilter(AbstractTestFixture):
    """
    Test if the filter on long waiting duration is working
    """

    def test_novalidjourney_on_first_call(self):
        """
        On this call the first call to kraken returns a journey
        with a too long waiting duration.
        The second call to kraken must return a valid journey
        """
        query = "journeys?from={from_sa}&to={to_sa}&datetime={datetime}"\
            .format(from_sa="A", to_sa="D", datetime="20120614T080000")

        response = self.query_region(query, display=False)
        eq_(len(response['journeys']), 1)
        logging.getLogger(__name__).info("arrival date: {}".format(response['journeys'][0]['arrival_date_time']))
        eq_(response['journeys'][0]['arrival_date_time'],  "20120614T160000")
        eq_(response['journeys'][0]['type'], "best")

    def test_novalidjourney_on_first_call_debug(self):
        """
        On this call the first call to kraken returns a journey
        with a too long waiting duration.
        The second call to kraken must return a valid journey
        We had a debug argument, hence 2 journeys are returned, only one is typed
        """
        query = "journeys?from={from_sa}&to={to_sa}&datetime={datetime}&debug=true"\
            .format(from_sa="A", to_sa="D", datetime="20120614T080000")

        response = self.query_region(query, display=False)
        eq_(len(response['journeys']), 2)
        eq_(response['journeys'][0]['arrival_date_time'], "20120614T150000")
        eq_(response['journeys'][0]['type'], "")
        eq_(response['journeys'][1]['arrival_date_time'], "20120614T160000")
        eq_(response['journeys'][1]['type'], "best")


    def test_remove_one_journey_from_batch(self):
        """
        Kraken returns two journeys, the earliest arrival one returns a too
        long waiting duration, therefore it must be deleted.
        The second one must be returned
        """
        query = "journeys?from={from_sa}&to={to_sa}&datetime={datetime}"\
            .format(from_sa="A", to_sa="D", datetime="20120615T080000")

        response = self.query_region(query, display=False)
        eq_(len(response['journeys']), 1)
        eq_(response['journeys'][0]['arrival_date_time'], u'20120615T151000')
        eq_(response['journeys'][0]['type'], "best")


    def test_max_attemps(self):
        """
        Kraken always retrieves journeys with non_pt_duration > max_non_pt_duration
        No journeys should be typed, but get_journeys should stop quickly
        """
        query = "journeys?from={from_sa}&to={to_sa}&datetime={datetime}"\
            .format(from_sa="E", to_sa="H", datetime="20120615T080000")

        response = self.query_region(query, display=False)
        assert(not "journeys" in response or len(response['journeys']) ==  0)


    def test_max_attemps_debug(self):
        """
        Kraken always retrieves journeys with non_pt_duration > max_non_pt_duration
        No journeys should be typed, but get_journeys should stop quickly
        We had the debug argument, hence a non-typed journey is returned
        """
        query = "journeys?from={from_sa}&to={to_sa}&datetime={datetime}&debug=true"\
            .format(from_sa="E", to_sa="H", datetime="20120615T080000")

        response = self.query_region(query, display=False)
        eq_(len(response['journeys']), 1)

@dataset(["main_routing_test"])
class TestShapeInGeoJson(AbstractTestFixture):
    """
    Test if the shape is used in the GeoJson
    """

    def test_shape_in_geojson(self):
        """
        Test if, in the first journey, the second section:
         - is public_transport
         - len of stop_date_times is 2
         - len of geojson/coordinates is 3 (and thus,
           stop_date_times is not used to create the geojson)
        """
        response = self.query_region(journey_basic_query, display=False)
        #print response['journeys'][0]['sections'][1]
        eq_(len(response['journeys']), 2)
        eq_(len(response['journeys'][0]['sections']), 3)
        eq_(response['journeys'][0]['co2_emission']['value'], 0.48)
        eq_(response['journeys'][0]['co2_emission']['unit'], 'gEC')
        eq_(response['journeys'][0]['sections'][1]['type'], 'public_transport')
        eq_(len(response['journeys'][0]['sections'][1]['stop_date_times']), 2)
        eq_(len(response['journeys'][0]['sections'][1]['geojson']['coordinates']), 3)
        eq_(response['journeys'][0]['sections'][1]['co2_emission']['value'], 0.48)
        eq_(response['journeys'][0]['sections'][1]['co2_emission']['unit'], 'gEC')

@dataset(["main_routing_test", "basic_routing_test"])
class TestOneDeadRegion(AbstractTestFixture):
    """
    Test if we still responds when one kraken is dead
    """

    def test_one_dead_region(self):
        self.krakens_pool["basic_routing_test"].kill()

        response = self.query("v1/journeys?from=stop_point:stopA&"
            "to=stop_point:stopB&datetime=20120614T080000&debug=true",
                              display=False)
        eq_(len(response['journeys']), 2)
        eq_(len(response['journeys'][0]['sections']), 1)
        eq_(response['journeys'][0]['sections'][0]['type'], 'public_transport')
        eq_(len(response['debug']['regions_called']), 1)
        eq_(response['debug']['regions_called'][0], "main_routing_test")


@dataset(["basic_routing_test"])
class TestIsochrone(AbstractTestFixture):
    def test_isochrone(self):
        response = self.query_region("journeys?from=I1&datetime=20120615T070000")
        assert(len(response['journeys']) == 2)


@dataset(["main_routing_without_pt_test", "main_routing_test"])
class TestWithoutPt(AbstractTestFixture):
    """
    Test if we still responds when one kraken is dead
    """

    def setup(self):
        from jormungandr import i_manager
        self.instance_map = {
            'main_routing_without_pt_test': MockKraken(i_manager.instances['main_routing_without_pt_test'], True),
            'main_routing_test': MockKraken(i_manager.instances['main_routing_test'], True),
        }
        self.real_method = models.Instance.get_by_name
        self.real_comparator = instance_manager.instances_comparator

        models.Instance.get_by_name = self.mock_get_by_name
        instance_manager.instances_comparator = lambda a, b: a.name == "main_routing_without_pt_test"

    def mock_get_by_name(self, name):
        return self.instance_map[name]

    def teardown(self):
        models.Instance.get_by_name = self.real_method
        instance_manager.instances_comparator = self.real_comparator

    def test_one_region_wihout_pt(self):
        response = self.query("v1/"+journey_basic_query+"&debug=true",
                              display=False)
        eq_(len(response['journeys']), 2)
        eq_(len(response['journeys'][0]['sections']), 3)
        eq_(response['journeys'][0]['sections'][1]['type'], 'public_transport')
        eq_(len(response['debug']['regions_called']), 2)
        eq_(response['debug']['regions_called'][0], "main_routing_without_pt_test")
        eq_(response['debug']['regions_called'][1], "main_routing_test")


