#!/usr/bin/env python
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
import importlib
from flask_restful.representations import json
from flask import request, make_response
from jormungandr import rest_api
from jormungandr.index import index
from jormungandr.modules_loader import ModulesLoader
import simplejson


@rest_api.representation("text/jsonp")
@rest_api.representation("application/jsonp")
def output_jsonp(data, code, headers=None):
    resp = json.output_json(data, code, headers)
    callback = request.args.get('callback', False)
    if callback:
        resp.data = str(callback) + '(' + resp.data + ')'
    return resp


@rest_api.representation("text/json")
@rest_api.representation("application/json")
def output_json(data, code, headers=None):
    resp = make_response(simplejson.dumps(data), code)
    resp.headers.extend(headers or {})
    return resp

# If modules are configured, then load and run them
if 'MODULES' in rest_api.app.config:
    rest_api.module_loader = ModulesLoader(rest_api)
    for prefix, module_info in rest_api.app.config['MODULES'].iteritems():
        module_file = importlib.import_module(module_info['import_path'])
        module = getattr(module_file, module_info['class_name'])
        rest_api.module_loader.load(module(rest_api, prefix))
    rest_api.module_loader.run()
else:
    rest_api.app.logger.warning('MODULES isn\'t defined in config. No module will be loaded, then no route '
                                'will be defined.')

index(rest_api)
