from data.abc.abstract_reader import AbstractReader
from data.simulation_data_type import SimulationDataType, generate_simulation_data_types
from data.event import Event

import csv
import re

class Reader(AbstractReader):
    """Parses event data CSV files as generated by FMITerminalBlock
    
    The object implements the reader semantic as defined by abc.abstract_reader.
    Events data files will be read as is without any filtering.
    """
       
    def __init__(self, csv_source):
        """Initializes the reader and registers the given source.
        
        The source_file may be anything which implements the iterator protocol.
        Most common usages include passing file objects and list of line-strings
        which contain the CSV data output of FMITerminalBlock. In case 
        csv_source is a file object, it should be called with newline='' 
        parameter [1].
        
        [1] https://docs.python.org/3/library/csv.html
        """
        
        # Quoting has to be handled manually to differentiate between  empty
        # fields and empty strings
        self._csv_reader = csv.reader(csv_source, \
            delimiter=";", lineterminator='\n', strict=True, \
            quoting=csv.QUOTE_NONE)
        self._header = {}
        self._name_list = []
        self._add_header()
    
    def _add_header(self):
        """Reads the first two header lines from the csv reader
        
        The information will be added to the header map.
        """
           
        (variables, types) = self._read_raw_header()
        (variables, types) = self._extract_variable_header(variables, types)
        
        sim_types = generate_simulation_data_types(types[1:])
        self._header.update(zip(variables[1:],sim_types))
        self._name_list.extend(variables[1:])
    
    def _read_raw_header(self):
        """Reads the header lines from the registered CSV reader
        
        The result is returned in a tuple which contains the packed but merged 
        type and value strings. as found in the CSV file
        """
        
        it = iter(self._csv_reader)
        try:
            variables = self._merge_columns(next(it))
            types = self._merge_columns(next(it))
            return (variables, types)
        except StopIteration:
            raise ValueError("The given CSV data source does not contain two "
                "header lines")
    
    def _extract_variable_header(self, variables, types):
        """Extracts the variable name and types
        
        The function parses the raw variable type list and generates an unpacked
        and normalized list of variable name and type strings. The resulting 
        pair of lists (variables, types) is basically validated and returned. No
        type checking of single strings is performed.
        """
        
        if len(variables) != len(types):
            raise ValueError(("The number of fields of the first header lines "
                "({}) does not match the number of fields of the second header "
                "line ({}). ").format(len(variables), len(types)))
        if len(variables) <= 0:
            raise ValueError("Empty CSV header found")
        if variables[0] != '"time"' or types[0] != '"fmiReal"':
            raise ValueError(("The first column is not a valid time reference. "
            "(variable is '{}', type is '{}')").format(variables[0], types[0]))
        
        if len(variables) == 2 and variables[1] == '':
            # No model variables set -> delimiter (;) is still present
            # -> Don't add an empty field
            variables = [variables[0]]
            types = [types[0]]
        
        return (self._unpack_all(variables), self._unpack_all(types))
    
    def get_header(self):
        """Returns the header map as defined by AbstractReader"""
        
        return self._header
    
    def __iter__(self):
        """Returns the iterator which constructs all events"""
        
        return self._generate_events()
    
    def _generate_events(self):
        """Generates the sequence of events until all events are consumed"""
        
        it = iter(self._csv_reader)
        try:
            while True:
                row = self._merge_columns(next(it))
                yield self._parse_row(row)
        except StopIteration:
            pass
    
    def _parse_row(self, row):
        """Parses the given data values and returns the generated event
        
        It is assumed that spurious row definitions are already merged
        """
        
        assert(len(self._name_list) == len(self._header))
        
        if len(row) != len(self._header) + 1 and \
            not (len(row) == 1 and len(self._header) == 0):
            raise ValueError("Invalid number of fields in {}".format(row))
        
        event = Event(row[0])
        
        for i in range(0,len(self._name_list)):
            varname = self._name_list[i]
            vartype = self._header[varname]
            self._parse_field(row[i+1], varname, vartype, event)
            
        return event
    
    def _parse_field(self, field, variable_name, variable_type, event):
        """Processes the given model variable and adds it to event
        
        In case the field is not populated, nothing will be added to the event
        variable. The field parameter corresponds to the raw but merged input 
        data, the variable_name names the official identifier and variable_type 
        contains the expected type of the field.
        """
        
        if field == "":
            return # Ignore unpopulated model variables
        
        if variable_type == SimulationDataType.REAL:
            event[variable_name] = float(field)
        elif variable_type == SimulationDataType.INTEGER:
            event[variable_name] = int(field)
        elif variable_type == SimulationDataType.BOOLEAN:
            event[variable_name] = (int(field) != 0)
        elif variable_type == SimulationDataType.STRING:
            event[variable_name] = self._unpack_string(field)
        else:
            assert(False)
    
    def _merge_columns(self, row):
        """Merges all columns which were falsely split by spurious splitting
        
        Since the CSV file differentiates between empty strings and empty 
        fields, quotation management needs to be done manually. The function 
        merges two columns in case the separation character occurs inside a 
        quoted string. The resulting row list is returned.
        """
        
        ret = []
        in_escape_sequence = False
        
        for field in row:
            if in_escape_sequence:
                ret.append(ret.pop() + ";" + field)
            else:
                ret.append(field)
            
            # Toggle flag, if an odd number of '"' characters is found
            in_escape_sequence = in_escape_sequence != (field.count('"') % 2 == 1)
        return ret
    
    def _unpack_string(self, value):
        """Removes the escape sequences from the string literal
        
        It is assumed that the literal is enclosed in double quotes.
        The resulting string is returned.
        """
        
        string_pattern = re.compile('^"(([^"]|"")*)"$')
        match = string_pattern.match(value)
        if not match:
            raise ValueError("The string '{}' is not properly escaped" \
                .format(value))
        return match.group(1).replace('""', '"')
    
    def _unpack_all(self, values):
        """Assumes that all items in the given list are strings and unpacks them
        
        Returns the resulting list
        """
        
        return [self._unpack_string(value) for value in values]
    
    