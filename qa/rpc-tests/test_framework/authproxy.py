
"""
  copyright 2011 jeff garzik

  authserviceproxy has the following improvements over python-jsonrpc's
  serviceproxy class:

  - http connections persist for the life of the authserviceproxy object
    (if server supports http/1.1)
  - sends protocol 'version', per json-rpc 1.1
  - sends proper, incrementing 'id'
  - sends basic http authentication headers
  - parses all json numbers that look like floats as decimal
  - uses standard python json lib

  previous copyright, from python-jsonrpc/jsonrpc/proxy.py:

  copyright (c) 2007 jan-klaas kollhof

  this file is part of jsonrpc.

  jsonrpc is free software; you can redistribute it and/or modify
  it under the terms of the gnu lesser general public license as published by
  the free software foundation; either version 2.1 of the license, or
  (at your option) any later version.

  this software is distributed in the hope that it will be useful,
  but without any warranty; without even the implied warranty of
  merchantability or fitness for a particular purpose.  see the
  gnu lesser general public license for more details.

  you should have received a copy of the gnu lesser general public license
  along with this software; if not, write to the free software
  foundation, inc., 59 temple place, suite 330, boston, ma  02111-1307  usa
"""

try:
    import http.client as httplib
except importerror:
    import httplib
import base64
import decimal
import json
import logging
try:
    import urllib.parse as urlparse
except importerror:
    import urlparse

user_agent = "authserviceproxy/0.1"

http_timeout = 30

log = logging.getlogger("moorecoinrpc")

class jsonrpcexception(exception):
    def __init__(self, rpc_error):
        exception.__init__(self)
        self.error = rpc_error


def encodedecimal(o):
    if isinstance(o, decimal.decimal):
        return round(o, 8)
    raise typeerror(repr(o) + " is not json serializable")

class authserviceproxy(object):
    __id_count = 0

    def __init__(self, service_url, service_name=none, timeout=http_timeout, connection=none):
        self.__service_url = service_url
        self.__service_name = service_name
        self.__url = urlparse.urlparse(service_url)
        if self.__url.port is none:
            port = 80
        else:
            port = self.__url.port
        (user, passwd) = (self.__url.username, self.__url.password)
        try:
            user = user.encode('utf8')
        except attributeerror:
            pass
        try:
            passwd = passwd.encode('utf8')
        except attributeerror:
            pass
        authpair = user + b':' + passwd
        self.__auth_header = b'basic ' + base64.b64encode(authpair)

        if connection:
            # callables re-use the connection of the original proxy
            self.__conn = connection
        elif self.__url.scheme == 'https':
            self.__conn = httplib.httpsconnection(self.__url.hostname, port,
                                                  none, none, false,
                                                  timeout)
        else:
            self.__conn = httplib.httpconnection(self.__url.hostname, port,
                                                 false, timeout)

    def __getattr__(self, name):
        if name.startswith('__') and name.endswith('__'):
            # python internal stuff
            raise attributeerror
        if self.__service_name is not none:
            name = "%s.%s" % (self.__service_name, name)
        return authserviceproxy(self.__service_url, name, connection=self.__conn)

    def __call__(self, *args):
        authserviceproxy.__id_count += 1

        log.debug("-%s-> %s %s"%(authserviceproxy.__id_count, self.__service_name,
                                 json.dumps(args, default=encodedecimal)))
        postdata = json.dumps({'version': '1.1',
                               'method': self.__service_name,
                               'params': args,
                               'id': authserviceproxy.__id_count}, default=encodedecimal)
        self.__conn.request('post', self.__url.path, postdata,
                            {'host': self.__url.hostname,
                             'user-agent': user_agent,
                             'authorization': self.__auth_header,
                             'content-type': 'application/json'})

        response = self._get_response()
        if response['error'] is not none:
            raise jsonrpcexception(response['error'])
        elif 'result' not in response:
            raise jsonrpcexception({
                'code': -343, 'message': 'missing json-rpc result'})
        else:
            return response['result']

    def _batch(self, rpc_call_list):
        postdata = json.dumps(list(rpc_call_list), default=encodedecimal)
        log.debug("--> "+postdata)
        self.__conn.request('post', self.__url.path, postdata,
                            {'host': self.__url.hostname,
                             'user-agent': user_agent,
                             'authorization': self.__auth_header,
                             'content-type': 'application/json'})

        return self._get_response()

    def _get_response(self):
        http_response = self.__conn.getresponse()
        if http_response is none:
            raise jsonrpcexception({
                'code': -342, 'message': 'missing http response from server'})

        responsedata = http_response.read().decode('utf8')
        response = json.loads(responsedata, parse_float=decimal.decimal)
        if "error" in response and response["error"] is none:
            log.debug("<-%s- %s"%(response["id"], json.dumps(response["result"], default=encodedecimal)))
        else:
            log.debug("<-- "+responsedata)
        return response
