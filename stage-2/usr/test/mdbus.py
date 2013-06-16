#!/usr/bin/env python
"""
Mickey's own dbus introspection utility.

(C) 2008-2009 Michael 'Mickey' Lauer <mlauer@vanille-media.de>

GPLv2 or later
"""

__version__ = "0.9.9.10"

from xml.parsers.expat import ExpatError, ParserCreate
from dbus.exceptions import IntrospectionParserException
import types

#----------------------------------------------------------------------------#
class _Parser(object):
#----------------------------------------------------------------------------#
# Copyright (C) 2003, 2004, 2005, 2006 Red Hat Inc. <http://www.redhat.com/>
# Copyright (C) 2003 David Zeuthen
# Copyright (C) 2004 Rob Taylor
# Copyright (C) 2005, 2006 Collabora Ltd. <http://www.collabora.co.uk/>
# Copyright (C) 2007 John (J5) Palmieri
#
# Permission is hereby granted, free of charge, to any person
# obtaining a copy of this software and associated documentation
# files (the "Software"), to deal in the Software without
# restriction, including without limitation the rights to use, copy,
# modify, merge, publish, distribute, sublicense, and/or sell copies
# of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
# HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
# WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    __slots__ = ('map',
                 'in_iface',
                 'in_method',
                 'in_signal',
                 'in_property',
                 'property_access',
                 'in_sig',
                 'out_sig',
                 'node_level',
                 'in_signal')
    def __init__(self):
        self.map = {'child_nodes':[],'interfaces':{}}
        self.in_iface = ''
        self.in_method = ''
        self.in_signal = ''
        self.in_property = ''
        self.property_access = ''
        self.in_sig = []
        self.out_sig = []
        self.node_level = 0

    def parse(self, data):
        parser = ParserCreate('UTF-8', ' ')
        parser.buffer_text = True
        parser.StartElementHandler = self.StartElementHandler
        parser.EndElementHandler = self.EndElementHandler
        parser.Parse(data)
        return self.map

    def StartElementHandler(self, name, attributes):
        if name == 'node':
            self.node_level += 1
            if self.node_level == 2:
                self.map['child_nodes'].append(attributes['name'])
        elif not self.in_iface:
            if (not self.in_method and name == 'interface'):
                self.in_iface = attributes['name']
        else:
            if (not self.in_method and name == 'method'):
                self.in_method = attributes['name']
            elif (self.in_method and name == 'arg'):
                arg_type = attributes['type']
                arg_name = attributes.get('name', None)
                if attributes.get('direction', 'in') == 'in':
                    self.in_sig.append({'name': arg_name, 'type': arg_type})
                if attributes.get('direction', 'out') == 'out':
                    self.out_sig.append({'name': arg_name, 'type': arg_type})
            elif (not self.in_signal and name == 'signal'):
                self.in_signal = attributes['name']
            elif (self.in_signal and name == 'arg'):
                arg_type = attributes['type']
                arg_name = attributes.get('name', None)

                if attributes.get('direction', 'in') == 'in':
                    self.in_sig.append({'name': arg_name, 'type': arg_type})
            elif (not self.in_property and name == 'property'):
                prop_type = attributes['type']
                prop_name = attributes['name']

                self.in_property = prop_name
                self.in_sig.append({'name': prop_name, 'type': prop_type})
                self.property_access = attributes['access']


    def EndElementHandler(self, name):
        if name == 'node':
            self.node_level -= 1
        elif self.in_iface:
            if (not self.in_method and name == 'interface'):
                self.in_iface = ''
            elif (self.in_method and name == 'method'):
                if not self.map['interfaces'].has_key(self.in_iface):
                    self.map['interfaces'][self.in_iface]={'methods':{}, 'signals':{}, 'properties':{}}

                if self.map['interfaces'][self.in_iface]['methods'].has_key(self.in_method):
                    print "ERROR: Some clever service is trying to be cute and has the same method name in the same interface"
                else:
                    self.map['interfaces'][self.in_iface]['methods'][self.in_method] = (self.in_sig, self.out_sig)

                self.in_method = ''
                self.in_sig = []
                self.out_sig = []
            elif (self.in_signal and name == 'signal'):
                if not self.map['interfaces'].has_key(self.in_iface):
                    self.map['interfaces'][self.in_iface]={'methods':{}, 'signals':{}, 'properties':{}}

                if self.map['interfaces'][self.in_iface]['signals'].has_key(self.in_signal):
                    print "ERROR: Some clever service is trying to be cute and has the same signal name in the same interface"
                else:
                    self.map['interfaces'][self.in_iface]['signals'][self.in_signal] = (self.in_sig,)

                self.in_signal = ''
                self.in_sig = []
                self.out_sig = []
            elif (self.in_property and name == 'property'):
                if not self.map['interfaces'].has_key(self.in_iface):
                    self.map['interfaces'][self.in_iface]={'methods':{}, 'signals':{}, 'properties':{}}

                if self.map['interfaces'][self.in_iface]['properties'].has_key(self.in_property):
                    print "ERROR: Some clever service is trying to be cute and has the same property name in the same interface"
                else:
                    self.map['interfaces'][self.in_iface]['properties'][self.in_property] = (self.in_sig, self.property_access)

                self.in_property = ''
                self.in_sig = []
                self.out_sig = []
                self.property_access = ''

#----------------------------------------------------------------------------#
def process_introspection_data(data):
#----------------------------------------------------------------------------#
    """Return a structure mapping all of the elements from the introspect data
       to python types TODO: document this structure

    :Parameters:
        `data` : str
            The introspection XML. Must be an 8-bit string of UTF-8.
    """
    try:
        return _Parser().parse(data)
    except Exception, e:
        raise IntrospectionParserException('%s: %s' % (e.__class__, e))

#----------------------------------------------------------------------------#
def dbus_to_python(v):
#----------------------------------------------------------------------------#
    class ObjectPath( object ):
        def __init__( self, path ):
            self.path = str( path )

        def __repr__( self ):
            return "op%s" % repr(self.path)

    if isinstance(v, dbus.Byte) \
        or isinstance(v, dbus.Int64) \
        or isinstance(v, dbus.UInt64) \
        or isinstance(v, dbus.Int32) \
        or isinstance(v, dbus.UInt32) \
        or isinstance(v, dbus.Int16) \
        or isinstance(v, dbus.UInt16) \
        or type(v) == types.IntType:
        return int(v)
    elif isinstance(v, dbus.Double) or type(v) == types.FloatType:
        return float(v)
    elif isinstance(v, dbus.String) or type(v) == types.StringType:
        return str(v)
    elif isinstance(v, dbus.Dictionary) or type(v) == types.DictType:
        return dict( (dbus_to_python(k), dbus_to_python(v)) for k,v in v.iteritems() )
    elif isinstance(v, dbus.Array) or type(v) == types.ListType:
        return [dbus_to_python(x) for x in v]
    elif isinstance(v, dbus.Struct) or type(v) == types.TupleType:
        return tuple(dbus_to_python(x) for x in v)
    elif isinstance(v, dbus.Boolean) or type(v) == types.BooleanType:
        return bool(v)
    elif isinstance(v, dbus.ObjectPath) or type(v) == ObjectPath:
        return ObjectPath(v)
    else:
        raise TypeError("Can't convert type %s to python object" % type(v))

#----------------------------------------------------------------------------#
def prettyPrint( expression ):
#----------------------------------------------------------------------------#
    from pprint import PrettyPrinter
    pp = PrettyPrinter( indent=4 )
    pp.pprint( dbus_to_python(expression) )

#----------------------------------------------------------------------------#
class Commands( object ):
#----------------------------------------------------------------------------#
    """
    Implementing the dbus introspection / interaction.
    """
    def __init__( self, bus ):
        if mode == "listen":
            self._setupMainloop()
        self.bus = bus()
        self.busname = None
        self.objpath = None
        self.rinterface = None

    def listBusNames( self ):
        names = self.bus.list_names()[:]
        names.sort()
        for n in names:
            print n

    def listObjects( self, busname ):
        self._listChildren( busname, '/' )

    def listMethods( self, busname, objname ):
        obj = self._tryObject( busname, objname )
        if obj is not None:
            iface = dbus.Interface( obj, "org.freedesktop.DBus.Introspectable" )
            self.data = data = process_introspection_data( iface.Introspect() )
            for name, interface in data["interfaces"].iteritems():
                self._listInterface( name, interface["signals"], interface["methods"], interface["properties"] )

    def callMethod( self, busname, objname, methodname, parameters=[] ):
        obj = self._tryObject( busname, objname )
        if obj is not None:

            if '.' in methodname:
                # if we have a fully qualified methodname, use an Interface
                ifacename = '.'.join( methodname.split( '.' )[:-1] )
                methodname = methodname.split( '.' )[-1]
                iface = dbus.Interface( obj, ifacename )
                method = getattr( iface, methodname )
            else:
                method = getattr( obj, methodname.split( '.' )[-1] )

            try:
                result = method( timeout=100000, *parameters )
            except dbus.DBusException, e:
                print "%s: %s failed: %s" % ( objname, methodname, e.get_dbus_name() )
            except TypeError, e:
                pass # python will emit its own error here
            else:
                if result is not None:
                    prettyPrint( result )

    def monitorBus( self ):
        self._runMainloop()

    def monitorService( self, busname ):
        self.busname = busname
        self._runMainloop()

    def monitorObject( self, busname, objname ):
        self.busname = busname
        self.objpath = objname
        self._runMainloop()

    #
    # command mode
    #

    def _listChildren( self, busname, objname ):
        fail = objname is '/'
        obj = self._tryObject( busname, objname, fail )
        iface = dbus.Interface( obj, "org.freedesktop.DBus.Introspectable" )
        print objname
        if obj is not None:
            data = process_introspection_data( iface.Introspect() )
            for o in data["child_nodes"]:
                newname = "%s/%s" % ( objname, o )
                newname = newname.replace( "//", "/" )
                self._listChildren( busname, newname )

    def _tryObject( self, busname, objname, fail=True ):
        try:
            obj = self.bus.get_object( busname, objname, introspect=True )
        except ( dbus.DBusException, ValueError ):
            if fail:
                if busname in self.bus.list_names():
                    print "Object name not found"
                else:
                    print "Service name not found"
                sys.exit( -1 )
            else:
                return None
        else:
            return obj

    def _parameter( self, type_, name ):
        return "%s:%s" % ( type_, name )

    def _signature( self, parameters ):
        string = "( "
        for p in parameters:
            string += self._parameter( p["type"], p["name"] )
            string += ", "
        if len( string ) == 2:
            return "()"
        else:
            return string[:-2] + " )"

    def _listInterface( self, name, signals, methods, properties ):
        methodnames = methods.keys()
        methodnames.sort()
        for mname in methodnames:
            inSignature = self._signature( methods[mname][0] )
            if returns:
                outSignature = self._signature( methods[mname][1] )
                print "[METHOD]    %s.%s%s -> %s" % ( name, mname, inSignature, outSignature )
            else:
                print "[METHOD]    %s.%s%s" % ( name, mname, inSignature )

        signalnames = signals.keys()
        signalnames.sort()
        for mname in signalnames:
            signature = self._signature( signals[mname][0] )
            print "[SIGNAL]    %s.%s%s" % ( name, mname, signature )

        propertynames = properties.keys()
        propertynames.sort()
        for mname in propertynames:
            signature = self._signature( properties[mname][0] )
            print "[PROPERTY]  %s.%s%s" % ( name, mname, signature )

    #
    # listening mode
    #

    def _setupMainloop( self ):
        import gobject
        import dbus.mainloop.glib
        dbus.mainloop.glib.DBusGMainLoop( set_as_default=True )
        self.mainloop = gobject.MainLoop()
        gobject.idle_add( self._setupListener )

    def _runMainloop( self ):
        try:
            b = self.bus.__class__.__name__
            bname = self.busname or "all"
            oname = self.objpath or "all"
            print "listening for signals on %s from service '%s', object '%s'..." % ( b, bname, oname )
            self.mainloop.run()
        except KeyboardInterrupt:
            self.mainloop.quit()
            sys.exit( 0 )

    def _setupListener( self ):
        self.bus.add_signal_receiver(
            self._signalHandler,
            None,
            None,
            self.busname,
            self.objpath,
            sender_keyword = "sender",
            destination_keyword = "destination",
            interface_keyword = "interface",
            member_keyword = "member",
            path_keyword = "path" )
        return False # don't call me again

    def _signalHandler( self, *args, **kwargs ):
        timestamp = time.strftime("%Y%m%d.%H%M.%S") if timestamps else ""
        print "%s [SIGNAL]    %s.%s    from %s %s" % ( timestamp, kwargs["interface"], kwargs["member"], kwargs["sender"], kwargs["path"] )
        prettyPrint( args )

#----------------------------------------------------------------------------#
if __name__ == "__main__":
#----------------------------------------------------------------------------#
    import gobject
    import dbus
    import sys
    import time
    import inspect

    argv = sys.argv[::-1]
    execname = argv.pop()

    if ( "-h" in argv ) or ( "--help" in argv ):
        print "Usage: %s [-s] [-l] [ busname [ objectpath [ methodname [ parameters... ] ] ] ]" % ( sys.argv[0] )
        sys.exit( 0 )

    bus = dbus.SessionBus
    mode = "command"
    timestamps = False
    escape = False
    returns = False

    # run through all arguments and check whether we got '-s' somewhere
    if "-s" in argv:
        bus = dbus.SystemBus
        argv.remove( "-s" )

    # run through all arguments and check whether we got '-l' somewhere
    if "-l" in argv:
        mode = "listen"
        argv.remove( "-l" )

    # run through all arguments and check whether we got '-t' somewhere
    if "-t" in argv:
        timestamps = True
        argv.remove( "-t" )

    # run through all arguments and check whether we got '-e' somewhere
    if "-e" in argv:
        escape = True
        argv.remove( "-e" )

    # run through all arguments and check whether we got '-s' somewhere
    if "-r" in argv:
        returns = True
        argv.remove( "-r" )

    c = Commands( bus )

    if len( argv ) == 0:
        if mode == "command":
            c.listBusNames()
        else:
            c.monitorBus()

    elif len( argv ) == 1:
        busname = argv.pop()
        if mode == "command":
            c.listObjects( busname )
        else:
            c.monitorService( busname )

    elif len( argv ) == 2:
        busname = argv.pop()
        objname = argv.pop()
        if mode == "command":
            c.listMethods( busname, objname )
        else:
            c.monitorObject( busname, objname )

    elif len( argv ) == 3:
        busname = argv.pop()
        objname = argv.pop()
        methodname = argv.pop()
        c.callMethod( busname, objname, methodname )

    else:
        busname = argv.pop()
        objname = argv.pop()
        methodname = argv.pop()
        parameters = []

        while argv:
            string = argv.pop()
            if string.strip() == "":
                parameters.append( "" )
                continue
            try:
                parameter = eval( string )
                if type( parameter ) == type( all ): # must be an error, we don't want FunctionOrMethod
                    raise SyntaxError
            except ( NameError, SyntaxError ): # treat as string
                parameter = eval( '"""%s"""' % string )
                if escape:
                    parameter = parameter.replace( '.r', '\r' )
                    parameter = parameter.replace( '.n', '\n' )
                parameters.append( parameter )
            except ( ValueError, AttributeError ), e:
                print "Error while evaluating '%s': %s" % ( string, e )
                sys.exit( -1 )
            else:
                parameters.append( parameter )

        c.callMethod( busname, objname, methodname, parameters )
