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

"""
Functions to launch the binaratisations
"""
from tyr.launch_exec import launch_exec
import logging
import os
import navitiacommon.task_pb2
import zipfile
from tyr import celery, redis
import datetime
from flask import current_app
import kombu
from navitiacommon import models
import shutil
from tyr.helper import get_instance_logger
from tyr.lock import Lock, retry_wrapper


def move_to_backupdirectory(filename, working_directory):
    """ If there is no backup directory it creates one in
        {instance_directory}/backup/{name}
        The name of the backup directory is the time when it's created
        formatted as %Y%m%d-%H%M%S
    """
    now = datetime.datetime.now()
    working_directory += "/" + now.strftime("%Y%m%d-%H%M%S%f")
    os.mkdir(working_directory, 0755)
    destination = working_directory + '/' + os.path.basename(filename)
    shutil.move(filename, destination)
    return destination


def make_connection_string(instance_config):
    """ Make a connection string connection from the config """
    connection_string = 'host=' + instance_config.pg_host
    connection_string += ' user=' + instance_config.pg_username
    connection_string += ' dbname=' + instance_config.pg_dbname
    connection_string += ' password=' + instance_config.pg_password
    return connection_string


@celery.task()
def fusio2ed(instance_config, filename, job_id):
    """ Unzip fusio file and launch fusio2ed """

    print "calling fusio2Ed"
    job = models.Job.query.get(job_id)
    instance = job.instance
    with Lock(instance.name, lambda: fusio2ed.retry(countdown=30, max_retries=100)):

        logger = get_instance_logger(instance)
        try:
            working_directory = os.path.dirname(filename)

            zip_file = zipfile.ZipFile(filename)
            zip_file.extractall(path=working_directory)

            params = ["-i", working_directory]
            if instance_config.aliases_file:
                params.append("-a")
                params.append(instance_config.aliases_file)

            if instance_config.synonyms_file:
                params.append("-s")
                params.append(instance_config.synonyms_file)

            connection_string = make_connection_string(instance_config)
            params.append("--connection-string")
            params.append(connection_string)
            res = launch_exec("fusio2ed", params, logger)
            if res != 0:
                raise ValueError('fusio2ed failed')
        except:
            logger.exception('')
            job.state = 'failed'
            models.db.session.commit()
            raise


@celery.task(bind=True)
def gtfs2ed(self, instance_config, gtfs_filename,  job_id):
    """ Unzip gtfs file launch gtfs2ed """

    job = models.Job.query.get(job_id)
    instance = job.instance
    with Lock(instance.name, retry_wrapper(self)):

        logger = get_instance_logger(instance)
        try:
            working_directory = os.path.dirname(gtfs_filename)

            zip_file = zipfile.ZipFile(gtfs_filename)
            zip_file.extractall(path=working_directory)

            params = ["-i", working_directory]
            if instance_config.aliases_file:
                params.append("-a")
                params.append(instance_config.aliases_file)

            if instance_config.synonyms_file:
                params.append("-s")
                params.append(instance_config.synonyms_file)

            connection_string = make_connection_string(instance_config)
            params.append("--connection-string")
            params.append(connection_string)
            res = launch_exec("gtfs2ed", params, logger)
            if res != 0:
                raise ValueError('gtfs2ed failed')
        except:
            logger.exception('')
            job.state = 'failed'
            models.db.session.commit()
            raise


@celery.task(bin=True)
def osm2ed(self, instance_config, osm_filename, job_id):
    """ launch osm2ed """

    job = models.Job.query.get(job_id)
    instance = job.instance
    with Lock(instance.name, retry_wrapper(self)):

        logger = get_instance_logger(instance)
        try:
            connection_string = make_connection_string(instance_config)
            res = launch_exec('osm2ed',
                    ["-i", osm_filename, "--connection-string", connection_string],
                    logger)
            if res != 0:
                #@TODO: exception
                raise ValueError('osm2ed failed')
        except:
            logger.exception('')
            job.state = 'failed'
            models.db.session.commit()
            raise

@celery.task(bind=True)
def geopal2ed(self, instance_config, filename, job_id):
    """ launch geopal2ed """

    job = models.Job.query.get(job_id)
    instance = job.instance
    with Lock(instance.name, retry_wrapper(self)):

        logger = get_instance_logger(instance)
        try:
            working_directory = os.path.dirname(filename)

            zip_file = zipfile.ZipFile(filename)
            zip_file.extractall(path=working_directory)

            connection_string = make_connection_string(instance_config)
            res = launch_exec('geopal2ed',
                    ["-i", working_directory, "--connection-string", connection_string],
                    logger)
            if res != 0:
                #@TODO: exception
                raise ValueError('geopal2ed failed')
        except:
            logger.exception('')
            job.state = 'failed'
            models.db.session.commit()
            raise

@celery.task(bind=True)
def poi2ed(self, instance_config, filename, job_id):
    """ launch poi2ed """

    job = models.Job.query.get(job_id)
    instance = job.instance
    with Lock(instance.name, retry_wrapper(self)):

        logger = get_instance_logger(instance)
        try:
            working_directory = os.path.dirname(filename)

            zip_file = zipfile.ZipFile(filename)
            zip_file.extractall(path=working_directory)

            connection_string = make_connection_string(instance_config)
            res = launch_exec('poi2ed',
                    ["-i", working_directory, "--connection-string", connection_string],
                    logger)
            if res != 0:
                #@TODO: exception
                raise ValueError('poi2ed failed')
        except:
            logger.exception('')
            job.state = 'failed'
            models.db.session.commit()
            raise

@celery.task(bind=True)
def synonym2ed(self, instance_config, filename, job_id):
    """ launch synonym2ed """

    job = models.Job.query.get(job_id)
    instance = job.instance
    with Lock(instance.name, retry_wrapper(self)):
        logger = get_instance_logger(instance)
        try:
            connection_string = make_connection_string(instance_config)
            res = launch_exec('synonym2ed',
                    ["-i", filename, "--connection-string", connection_string],
                    logger)
            if res != 0:
                #@TODO: exception
                raise ValueError('synonym2ed failed')
        except:
            logger.exception('')
            job.state = 'failed'
            models.db.session.commit()
            raise

@celery.task()
def reload_data(instance_config, job_id):
    """ reload data on all kraken of this instance"""
    job = models.Job.query.get(job_id)
    instance = job.instance
    logging.info("Unqueuing job {}, reload data of instance {}".format(job.id, instance.name))
    logger = get_instance_logger(instance)
    try:
        task = navitiacommon.task_pb2.Task()
        task.action = navitiacommon.task_pb2.RELOAD

        connection = kombu.Connection(current_app.config['CELERY_BROKER_URL'])
        exchange = kombu.Exchange(instance_config.exchange, 'topic',
                                  durable=True)
        producer = connection.Producer(exchange=exchange)

        logger.info("reload kraken")
        producer.publish(task.SerializeToString(),
                routing_key=instance.name + '.task.reload')
        connection.release()
    except:
        logger.exception('')
        job.state = 'failed'
        models.db.session.commit()
        raise


@celery.task(bind=True)
def ed2nav(self, instance_config, job_id):
    """ Launch ed2nav"""
    job = models.Job.query.get(job_id)
    instance = job.instance
    with Lock(instance.name, retry_wrapper(self)):
        logger = get_instance_logger(instance)
        try:
            filename = instance_config.tmp_file
            connection_string = make_connection_string(instance_config)
            argv = ["-o", filename, "--connection-string", connection_string]
            if 'CITIES_DATABASE_URI' in current_app.config:
                argv.extend(["--cities-connection-string", current_app.config['CITIES_DATABASE_URI']])
            res = launch_exec('ed2nav', argv, logger)
            if res != 0:
                raise ValueError('ed2nav failed')
        except:
            logger.exception('')
            job.state = 'failed'
            models.db.session.commit()
            raise


@celery.task(bind=True)
def nav2rt(self, instance_config, job_id):
    """ Launch nav2rt"""
    job = models.Job.query.get(job_id)
    instance = job.instance
    with Lock(instance.name, retry_wrapper(self)):

        logger = get_instance_logger(instance)
        try:
            source_filename = instance_config.tmp_file
            target_filename = instance_config.target_file
            connection_string = make_connection_string(instance_config)
            res = launch_exec('nav2rt',
                        ["-i", source_filename, "-o", target_filename,
                            "--connection-string", connection_string],
                        logger)
            if res != 0:
                raise ValueError('nav2rt failed')
        except:
            logger.exception('')
            job.state = 'failed'
            models.db.session.commit()
            raise


@celery.task(bind=True)
def fare2ed(self, instance_config, filename, job_id):
    """ launch fare2ed """

    print "calling fare2Ed"
    job = models.Job.query.get(job_id)
    instance = job.instance
    with Lock(instance.name, lambda: self.retry(countdown=3, max_retries=100)):
        logger = get_instance_logger(instance)
        try:
            working_directory = os.path.dirname(filename)

            zip_file = zipfile.ZipFile(filename)
            zip_file.extractall(path=working_directory)

            res = launch_exec("fare2ed", ['-f', working_directory,
                                          '--connection-string',
                                          make_connection_string(instance_config)],
                              logger)
            if res != 0:
                #@TODO: exception
                raise ValueError('fare2ed failed')
        except:
            logger.exception('')
            job.state = 'failed'
            models.db.session.commit()
            raise