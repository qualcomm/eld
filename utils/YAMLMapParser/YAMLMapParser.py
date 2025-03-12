#!/pkg/qct/software/python/3.4.0/bin/python

import argparse
import os
import sys
import yaml
try:
    from yaml import CLoader as Loader
except ImportError:
    print("Warning: Missing CLoader - long run time is likely.")
    from yaml import Loader

class YAMLFile(object):

    empty_totals = {
        "code": 0,
        "data": 0,
        "ro_data": 0,
        "rw_data": 0,
        "zi_data": 0,
        "code_no_load": 0,
        "data_no_load": 0,
        "ro_data_no_load": 0,
        "rw_data_no_load": 0,
        "zi_data_no_load": 0,
        "debug": 0,
        "no_load": 0,
    }

    def __init__(self, yamlfile):
        self.yamlfile = yaml.load(yamlfile, Loader=Loader)
        self.grand_totals = {}
        self.objs = {}
        self.obj_totals = {}
        self.linker_script = {
            'zi_data': 0
        }
        self.lib_objs = {}
        self.lib_totals = {}
        self.libs = {}
        self.generated = {}
        self.unused = {}
        self.object_map_code = {}
        self.unused_symbols = []
        self.loaded_sections = set()
        if 'LoadRegions' not in self.yamlfile:
            return
        for region in self.yamlfile['LoadRegions']:
            if 'Sections' not in region or 'PT_LOAD' not in region['Type']:
                continue
            for section in region['Sections']:
                self.loaded_sections.add(section)

    def get_architecture(self):
        '''Return Header->Architecture.'''
        return self.yamlfile['Header']['Architecture']

    def get_abi_page_size(self) -> int:
        '''Return Header->ABIPageSize.'''
        return self.yamlfile['Header']['ABIPageSize']

    def get_emulation(self):
        '''Return Header->Emulation.'''
        return self.yamlfile['Header']['Emulation']

    def get_address_size(self):
        '''Return Header->AddressSize.'''
        return self.yamlfile['Header']['AddressSize']

    def is_no_load(self, osection):
        '''Return True if output section is not loaded otherwise False'''
        if 'Name' not in osection:
            return False
        return osection['Name'] not in self.loaded_sections

    def is_code(self, item):
        '''Return True if item is code, otherwise return False.'''
        res = False
        if 'Type' in item and 'Permissions' in item:
            permission = item['Permissions']
            if ('SHT_PROGBITS' in item['Type'] and
                'SHF_ALLOC' in permission and
                'SHF_EXECINSTR' in permission and
                not 'SHT_REL' in item['Type']):
                res = True
        return res

    def is_ro_data(self, item):
        '''Return True if item is RO data, otherwise return False.'''
        res = False
        if 'Type' in item and 'Permissions' in item:
            permission = item['Permissions']
            if ('SHT_PROGBITS' in item['Type'] and
                'SHF_ALLOC' in permission and
                not 'SHF_WRITE' in permission and
                not 'SHF_EXECINSTR' in permission and
                not 'SHT_REL' in item['Type']):
                res = True
        return res

    def is_rw_data(self, item):
        '''Return True if item is RW data, otherwise return False.'''
        res = False
        if 'Type' in item and 'Permissions' in item:
            permission = item['Permissions']
            if ('SHT_PROGBITS' in item['Type'] and
                'SHF_ALLOC' in permission and
                'SHF_WRITE' in permission and
                not 'SHF_EXECINSTR' in permission and
                not 'SHT_REL' in item['Type']):
                res = True
        return res

    def is_zi_data(self, item):
        '''Return True if item is ZI data, otherwise return False.'''
        res = False
        if 'Type' in item and 'Permissions' in item:
            permission = item['Permissions']
            if ('SHT_NOBITS' in item['Type'] and
                'SHF_ALLOC' in permission and
                'SHF_WRITE' in permission and
                not 'SHF_EXECINSTR' in permission and
                not 'SHT_REL' in item['Type']):
                res = True
        return res

    def is_debug(self, item):
        '''Return True if item is debug, otherwise return False.'''
        res = False
        if 'Type' in item and 'Permissions' in item:
            permission = item['Permissions']
            if ('SHT_PROGBITS' in item['Type'] and
                not 'SHF_ALLOC' in permission and
                not 'SHF_WRITE' in permission and
                not 'SHF_EXECINSTR' in permission and
                not 'SHT_REL' in item['Type']):
                name = self.get_name(item)
                if name.startswith('.debug_'):
                    res = True
        return res

    def get_section_type(self, section):
        '''Return section type.'''
        if self.is_code(section):
            return 'code'
        elif self.is_ro_data(section):
            return 'ro_data'
        elif self.is_rw_data(section):
            return 'rw_data'
        elif self.is_zi_data(section):
            return 'zi_data'
        elif self.is_debug(section):
            return 'debug'
        else:
            return None

    def is_lib_member(self, item):
        '''Return True if item is a member of some library, otherwise return
        False.
        '''
        origin = self.get_origin(item)
        if '(' in origin and origin.endswith(')'):
            return True
        else:
            return False

    def get_name(self, item):
        '''Return item->Name.'''
        if 'Name' in item:
            return item['Name']
        else:
            return ''

    def get_size(self, item):
        '''Return item->Size.'''
        if 'Size' in item:
            return item['Size']
        elif 'Padding' in item:
            return item['Padding']
        else:
            return 0

    def get_origin(self, item):
        '''Return item->Origin.'''
        if 'Origin' in item:
            return item['Origin']
        elif 'IRFile' in item:
            return item['IRFile']
        else:
            return None

    def get_from(self, item):
        '''Return item->From.'''
        if 'From' in item:
            return item['From']
        else:
            return None

    def get_to(self, item):
        '''Return item->To.'''
        if 'To' in item:
            return item['To']
        else:
            return None

    def get_tosection(self, item):
        '''Return item->ToSection.'''
        if 'ToSection' in item:
            return item['ToSection']
        else:
            return None

    def get_grand_totals(self):
        '''Return grand_totals.'''
        if self.grand_totals:
            return self.grand_totals
        self.grand_totals = self.empty_totals.copy()
        obj_totals = self.get_object_totals()
        lib_totals = self.get_library_totals()
        self.grand_totals = dict(obj_totals)
        for k in self.grand_totals:
            self.grand_totals[k] += lib_totals[k]
        self.grand_totals['zi_data'] += self.linker_script['zi_data']
        return self.grand_totals

    def update_linker_script_size(self, section, sec_type, size):
        self.linker_script['zi_data'] += section['Size']

    def update_object_sizes_no_content(self, section, sec_type):
        if not self.generated:
            self.generated = self.empty_totals.copy()
        self.generated[sec_type] += self.get_size(section)
        no_load = self.is_no_load(section)
        if no_load:
            self.generated["no_load"] += self.get_size(section)

    def update_object_sizes(self, section, sec_type, no_load):
        '''Update size of objs, lib_objs and generated'''
        origin = self.get_origin(section)
        if 'Name' not in section:
            return False
        section_size = self.get_size(section)
        if section['Name'] in {"ALIGNMENT_PADDING", "LINKER_SCRIPT_PADDING"} :
            if not self.generated:
                self.generated = self.empty_totals.copy()
            self.generated[sec_type] += section_size
            if no_load:
                self.generated["no_load"] += section_size
            return True
        elif origin is not None and self.is_lib_member(section):
            if not origin in self.lib_objs:
                self.lib_objs[origin] = self.empty_totals.copy()
            self.lib_objs[origin][sec_type] += section_size
            if no_load:
                self.lib_objs[origin]["no_load"] += section_size
            return True
        elif origin is not None:
            if not origin in self.objs:
                self.objs[origin] = self.empty_totals.copy()
            self.objs[origin][sec_type] += section_size
            if no_load:
                self.objs[origin]["no_load"] += section_size
            return True
        return False

    def get_object_sizes(self):
        '''Populate objs, lib_objs and generated, and return objs.'''
        if self.objs:
            return self.objs
        for osection in self.yamlfile['OutputSections']:
            sec_type = self.get_section_type(osection)
            if not sec_type:
                continue
            no_load = self.is_no_load(osection)
            if no_load and sec_type != "debug":
                sec_type += "_no_load"
            ret = False
            if osection['Contents'] is None:
                self.update_object_sizes_no_content(osection, sec_type)
                continue
            for isection in osection['Contents']:
                new_sec_type = self.get_section_type(isection)
                if new_sec_type is None:
                    new_sec_type = sec_type
                elif no_load and sec_type != "debug":
                    new_sec_type += "_no_load"
                newret = self.update_object_sizes(isection, new_sec_type, no_load)
                if newret is True:
                    ret = newret
            # Check if all of the content was able to be parsed. If the content
            # cannot be parsed, the size of the payload is the size of the
            # output section.
            if ret is False:
                self.update_linker_script_size(osection, sec_type, osection['Size'])

        return self.objs

    def is_contents(self, item):
        '''Return True if item is contents, otherwise return False.'''
        return 'Contents' in item

    def get_permissions(self, item):
        '''Return item->Permissions.'''
        return item.get('Permissions', '')

    def get_symbols(self, item):
        '''Return item->Symbols.'''
        if 'Symbols' in item:
            return item['Symbols']
        else:
            return None

    def get_symbols_type(self, item):
        '''Return item->Type.'''
        return item.get('Type', '')

    def get_symbols_symbol(self, item):
        '''Return item->Symbol.'''
        return item.get('Symbol', '')

    def get_symbols_value(self, item):
        '''Return item->Value.'''
        return item.get('Value', '')

    def get_symbols_size(self, item):
        '''Return item->Size.'''
        return item.get('Size', '')
    def get_address(self, item):
        '''Return item->Address.'''
        return item.get('Address', 0)

    def get_offset(self, item):
        '''Return item->Offset.'''
        return item.get('Offset', 0)

    def get_attr(self, item):
        '''Return item->attr.'''
        permission = self.get_permissions(item)
        if self.is_rw_data(item):
            return 'RW'
        elif self.is_ro_data(item):
            return 'RO'
        elif self.is_code(item) and not 'SHF_WRITE' in permission:
            return 'RO'
        elif self.is_zi_data(item):
            return 'RW'
        else:
            return '0'

    def get_type(self, item):
        '''Return item->Type.'''
        if self.is_code(item):
            return 'CODE'
        else:
            return 'DATA'

    def get_object_map_code(self, item):
        '''entry address code .'''
        i = 0
        total = 0
        self.object_map_code = {}
        for osection in self.yamlfile['OutputSections']:
            section_name = osection.get('Name', "")
            if self.is_contents(osection) and item == section_name:
                if osection['Contents'] is not None:
                    base = self.get_address(osection)
                    for isection in osection['Contents']:
                        addr = base + self.get_offset(isection)
                        size = self.get_size(isection)
                        type = self.get_type(isection)
                        attr = self.get_attr(isection)
                        e_section_name = self.get_name(isection)
                        origin = self.get_origin(isection)
                        permission = self.get_permissions(isection)

                        if (origin is not None and 'SHF_ALLOC' in permission and
                            item == section_name):
                            if self.object_map_code is None:
                                self.object_map_code[i] = {
                                    'addr': 0,
                                    'size': 0,
                                    'type': 0,
                                    'attr': 0,
                                    's_name': 0,
                                    'origin': 0,
                                    'totalSize': 0
                                }
                            else:
                                total = total + size
                                self.object_map_code[i] = {
                                    'addr': addr,
                                    'size': size,
                                    'type': type,
                                    'attr': attr,
                                    's_name': e_section_name,
                                    'origin': origin,
                                    'totalSize': total
                                }
                                i += 1
                else:
                    return None
        if (len(self.object_map_code) == 0):
          return None
        return self.object_map_code

    def get_object_map(self):
        '''entry address code .'''
        entry_point = self.yamlfile['EntryAddress']
        print("Image Entry point: {0:#010x}".format(entry_point))
        for osection in self.yamlfile['LoadRegions']:
            if 'Sections' in osection and osection['Sections'] is not None:
                print("\n\n")
                print("Load Regions %s   Base: %08x, Size: %08x, Max: %08x\n" % (osection['Name'],
                    osection['VirtualAddress'], osection['FileSize'], osection['MemorySize'] ))
                for isection in osection['Sections']:
                    object_map = self.get_object_map_code(isection)
                    if object_map is not None:
                        if len (object_map) == 0:
                            print ("\n Execution Region %s   Base: %08x, Size: %08x\n" % (isection, osection['VirtualAddress'], osection['FileSize'] ))
                            print (" **** No section assigned to this execution region ****\n")
                        else:
                            print ("\n Execution Region  %s  Base: %08x, Size: %08x\n" % (isection , object_map[0]['addr'], object_map[len (object_map)-1]['totalSize'] ))
                            print("     Base Addr       Size         Type   Attr      E Section Name                        Object")
                            for k, v in object_map.items():
                                print("    {0:#010x}   {1:#011x}      {2:6s}  {3:6s}  {4:35s}     {5:11s}".format(
                                v['addr'], v['size'], v['type'],v['attr'], v['s_name'],
                                os.path.basename(v['origin'])))

    def get_object_symbols(self):
        '''symbols detail infomation .'''
        print ("Symbol Name           Value      Ov Type        Size       Object(Section)")
        for osection in self.yamlfile['OutputSections']:
            if 'Contents' in osection and osection['Contents'] is not None:
                for isection in osection['Contents']:
                    obj_name = self.get_name(isection)
                    origin = self.get_origin(isection)
                    symbols = self.get_symbols(isection)
                    if symbols is not None:
                        for symbol_detail in isection['Symbols']:
                            symbol_symbol = self.get_symbols_symbol(symbol_detail)
                            symbol_value = self.get_symbols_value(symbol_detail)
                            symbol_type = self.get_symbols_type(symbol_detail)
                            symbol_size = self.get_symbols_size(symbol_detail)
                            print ("   %s         %08x        %s      %08x       %s\n" % (symbol_symbol, symbol_value, symbol_type, symbol_size, origin ))

    def get_section_symbols(self, item):
        '''entry address code .'''
        i = 0
        self.object_map_code = {}
        for osection in self.yamlfile['OutputSections']:
            section_name = osection.get('Name', "")
            if 'Contents' in osection and osection['Contents'] is not None:
                base = self.get_address(osection)
                for isection in osection['Contents']:
                    e_section_name = self.get_name(isection)
                    if (item == e_section_name):
                        symbols = self.get_symbols(isection)
                        if symbols is not None:
                          for symbol_detail in isection['Symbols']:
                              symbol_symbol = self.get_symbols_symbol(symbol_detail)
                              symbol_value = self.get_symbols_value(symbol_detail)
                              symbol_type = self.get_symbols_type(symbol_detail)
                              symbol_size = self.get_symbols_size(symbol_detail)
                              '''print "   %s  %s  %s  %d\n" % (symbol_symbol, symbol_value, symbol_type, symbol_size )'''
                              if (symbol_size != 0):
                                self.object_map_code[i] = {
                                    'function': symbol_symbol,
                                    'size': symbol_size
                                }
                                i += 1
        if (len(self.object_map_code) == 0):
          return None
        return self.object_map_code

    def get_trampolines(self):
        '''trampolines detail infomation .'''
        self.trampoline_set = {}
        i = 0
        for osection in self.yamlfile['Trampolines']:
            if 'OutputSection' in osection and osection['OutputSection'] is not None:
              osection_name = osection['OutputSection']
            else:
              osection_name = "Unknown_section"
            if 'Trampolines' in osection and osection['Trampolines'] is not None:
                for isection in osection['Trampolines']:
                    name = self.get_name(isection)
                    caller_section = self.get_from(isection)
                    callee = self.get_to(isection)
                    ''' Get all symbols for caller section '''
                    object_map = self.get_section_symbols(caller_section)
                    if object_map is not None:
                        '''This happens when no -ffunction-sections was used.'''
                        if (len(object_map) > 1):
                          print ("%s %s %s %s multiple_callers 0 0" % (osection_name , name, caller_section, callee ))
                        else:
                          ''' Actually only one symbol in the section. '''
                          for k, v in object_map.items():
                            if 'Uses' in isection and isection['Uses'] is not None:
                              print ("%s %s %s %s %d %s %d" % (osection_name , name, caller_section, v['function'], v['size'], callee, len(isection['Uses'])))
                            else:
                              print ("%s %s %s %s %d %s 0" % (osection_name , name, caller_section, v['function'], v['size'], callee ))
                            self.trampoline_set[i] = {'osection' : osection_name, 'caller' : v['function'], 'calee': callee }
                            i += 1
                    else:
                        print ("%s %s %s %s missing_caller_symbols" % (osection_name , name, caller_section, callee ))
                    '''Now, if we have multiple users of this trampoline, try to resolve that part. '''
                    if 'Uses' in isection and isection['Uses'] is not None:
                      print ("reuses:")
                      for users in isection['Uses']:
                        user = self.get_from(users)
                        object_map = self.get_section_symbols(user)
                        if object_map is not None:
                          for k, v in object_map.items():
                            print ("  %s %s %d %s" % ( user, v['function'], v['size'], callee ))
                            self.trampoline_set[i] = {'osection' : osection_name, 'caller' : v['function'], 'calee': callee }
                            i += 1
        return self.trampoline_set

    def get_trampolines_without_sections(self):
        '''trampolines detail infomation .'''
        self.trampoline_set = {}
        i = 0
        for osection in self.yamlfile:
            osection_name = osection['OutputSection']
            if 'Trampolines' in osection and osection['Trampolines'] is not None:
                for isection in osection['Trampolines']:
                    name = self.get_name(isection)
                    caller_section = self.get_from(isection)
                    callee_to = self.get_to(isection)
                    callee_section = self.get_tosection(isection)
                    callee_list = []
                    print ("%s %s %s %s %s" % (osection_name , name, caller_section, callee_to, callee_section))
                    # Gather calees list
                    if 'ToSymbols' in isection and isection['ToSymbols'] is not None:
                      for clesym in isection['ToSymbols']:
                        type = clesym['Type']
                        if 'STT_SECT' in type:
                          continue
                        if 'STT_FUNC' in type:
                          callee_list.append(clesym['Symbol'])
                    # Only if there is no ToSymbols: entry, use the value of To:
                    else:
                      callee_list.append(callee_to)
                    # Visit callers
                    if 'FromSymbols' in isection and isection['FromSymbols'] is not None:
                      caller_symbol = "no-csym"
                      for csym in isection['FromSymbols']:
                        type = csym['Type']
                        # We can have ['STT_OBJECT', 'STT_FUNC', 'STT_SECT'] .text.foo
                        if 'STT_SECT' in type:
                          continue
                        if 'STT_FUNC' in type:
                          caller_symbol = csym['Symbol']
                          print ("  from %s" % (caller_symbol))
                          # Now for each caller symbol, iterate through calee list.
                          for callee_symbol in callee_list:
                            print ("  to   %s" % (callee_symbol))
                            self.trampoline_set[i] = {'osection' : osection_name, 'caller' : caller_symbol, 'calee': callee_symbol, 'tosection': callee_section, 'name': name }
                            i += 1
                    # Now, if we have multiple users of this trampoline, try to resolve that part.
                    if 'Uses' in isection and isection['Uses'] is not None:
                      print ("Reuses %d:" % (len(isection['Uses'])))
                      for users in isection['Uses']:
                        user = self.get_from(users)
                        if 'Symbols' in users and users['Symbols'] is not None:
                          user_caller_symbol = "no-user-symbol"
                          for symbol in users['Symbols']:
                            type = symbol['Type']
                            if 'STT_SECT' in type:
                              continue
                            if 'STT_FUNC' in type:
                              user_caller_symbol = symbol['Symbol']
                              for callee_symbol in callee_list:
                                print ("  %s %s %s" % ( user, user_caller_symbol, callee_symbol ))
                                self.trampoline_set[i] = {'osection' : osection_name, 'caller' : user_caller_symbol, 'calee': callee_symbol, 'tosection': callee_section, 'name': name }
                                i += 1
        return self.trampoline_set

    def get_trampolines_stats(self):
        '''trampolines stats'''
        self.unique_trampoline_stat = {}
        self.reuse_trampoline_stat = {}
        for osection in self.yamlfile:
            osection_name = osection['OutputSection']
            if 'Trampolines' in osection and osection['Trampolines'] is not None:
                for isection in osection['Trampolines']:
                    callee_to = self.get_to(isection)
                    callee_list = []
                    # Gather calees list
                    if 'ToSymbols' in isection and isection['ToSymbols'] is not None:
                      for clesym in isection['ToSymbols']:
                        type = clesym['Type']
                        if 'STT_SECT' in type:
                          continue
                        if 'STT_FUNC' in type:
                          callee_list.append(clesym['Symbol'])
                    # Only if there is no ToSymbols: entry, use the value of To:
                    else:
                      callee_list.append(callee_to)
                    # Visit callers
                    if 'FromSymbols' in isection and isection['FromSymbols'] is not None:
                      for csym in isection['FromSymbols']:
                        type = csym['Type']
                        # We can have ['STT_OBJECT', 'STT_FUNC', 'STT_SECT'] .text.foo
                        if 'STT_SECT' in type:
                          continue
                        if 'STT_FUNC' in type:
                          # Now for each caller symbol, iterate through calee list.
                          for callee_symbol in callee_list:
                            if osection_name in self.unique_trampoline_stat:
                              self.unique_trampoline_stat[osection_name] += 1
                            else:
                              self.unique_trampoline_stat[osection_name] = 1
                    # Now, if we have multiple users of this trampoline, try to resolve that part.
                    if 'Uses' in isection and isection['Uses'] is not None:
                      for users in isection['Uses']:
                        if 'Symbols' in users and users['Symbols'] is not None:
                          for symbol in users['Symbols']:
                            type = symbol['Type']
                            if 'STT_SECT' in type:
                              continue
                            if 'STT_FUNC' in type:
                              for callee_symbol in callee_list:
                                if osection_name in self.reuse_trampoline_stat:
                                  self.reuse_trampoline_stat[osection_name] += 1
                                else:
                                  self.reuse_trampoline_stat[osection_name] = 1
        return self.unique_trampoline_stat, self.reuse_trampoline_stat

    def get_reference(self):
        '''section reference code .'''
        for osection in self.yamlfile['CrossReference']:
            if osection is not None:
                refer_to = osection.get('Symbol', "")
                refer_file = osection.get('ReferencedBy', "")
                print (" %s refers to %s" % (refer_file, refer_to))

    def get_generated_sizes(self):
        '''Return generated.'''
        if not self.generated:
            self.generated = self.empty_totals.copy()
        return self.generated

    def get_library_member_sizes(self):
        '''Return lib_objs.'''
        if not self.lib_objs:
            self.get_object_sizes()
        return self.lib_objs

    def get_library_sizes(self):
        '''Return libs.'''
        if self.libs:
            return self.libs
        lib_objs = self.get_library_member_sizes()
        for k, v in lib_objs.items():
            libname = get_library_name(k)
            if not libname in self.libs:
                self.libs[libname] = self.empty_totals.copy()
            for t in v:
                self.libs[libname][t] += v[t]
        return self.libs

    def get_object_totals(self):
        '''Return obj_totals.'''
        if self.obj_totals:
            return self.obj_totals
        objs = self.get_object_sizes()
        generated = self.get_generated_sizes()
        self.obj_totals = dict(generated)
        for k, v in objs.items():
            for t in v:
                self.obj_totals[t] += v[t]
        return self.obj_totals

    def get_library_totals(self):
        '''Return lib_totals.'''
        if self.lib_totals:
            return self.lib_totals
        self.lib_totals = self.empty_totals.copy()
        lib_objs = self.get_library_member_sizes()
        for k, v in lib_objs.items():
            for t in v:
                self.lib_totals[t] += v[t]
        return self.lib_totals

    def get_unused(self):
        '''Return unsued sections.'''
        if self.unused:
            return self.unused
        for section in self.yamlfile['DiscardedSections']:
            self.unused[self.get_name(section)] = {
                'origin': self.get_origin(section),
                'size': self.get_size(section)
                }
        return self.unused

    def get_unused_symbols(self):
        '''Return unused symbols.'''
        if self.unused_symbols:
            return self.unused_symbols
        for section in self.yamlfile['DiscardedSections']:
            if 'Symbols' in section:
                for sym in section['Symbols']:
                    self.unused_symbols.append(sym['Symbol'])
        return self.unused_symbols

    def get_unused_objects(self):
        '''Return unused objects.'''
        for section in self.yamlfile['InputInfo']:
            section_used = section.get('Used', "")
            if section_used == 'NotUsed':
                file_path = section.get('Path', "")
                print ("  Unused objects: %s" % file_path)
                if 'Members' in section:
                    member = section.get('Members', "")
                    for sub_section in member:
                        section_used = sub_section.get('Used', "")
                        if section_used == 'NotUsed':
                            file_path = sub_section.get('Path', "")
                            print ("  Unused objects:     %s" %file_path)
            else:
                if 'Members' in section:
                    unused = True
                    lib_file_path = section.get('Path', "")
                    member = section.get('Members', "")
                    for sub_section in member:
                        section_used = sub_section.get('Used', "")
                        if section_used == 'NotUsed':
                            if unused == True:
                                print ("  Unused objects: %s" % lib_file_path)
                                unused = False
                            file_path = sub_section.get('Path', "")
                            print ("  Unused objects:    %s" % file_path)

    def has_key_word(self, item):
        '''Return True if item is in file, otherwise return False.'''
        return item in self.yamlfile


def get_library_name(s):
    '''Return library name.'''
    return s[0:s.find("(")]


def get_library_member_name(s):
    '''Return library member name.'''
    return s[s.find("(")+1:s.find(")")]


def handle_map(yamlfile):
    '''Handle --map'''
    keyword1 = 'LoadRegions'
    keyword2 = 'OutputSections'
    keyword3 = 'EntryAddress'
    if (yamlfile.has_key_word(keyword1) and yamlfile.has_key_word(keyword2) and
        yamlfile.has_key_word(keyword3)):
        print("Memory Map of the image\n")
        yamlfile.get_object_map()
    else:
        print ("No information about map load regions\n")

def handle_symbols_xx(yamlfile):
    '''Handle --symbols'''
    keyword1 = 'OutputSections'
    if (yamlfile.has_key_word(keyword1)):
        print("Image Symbol Table\n")
        print("   Local Symbols\n")
        yamlfile.get_object_symbols()
    else:
        print ("No information about Symbol Table\n")

def handle_trampolines(yamlfile, tramp_map_file, tramp_stat_file, trampolines_section_filter_file_name):
    '''Handle --trampolines
       Final file format is _tramp_entry_ caller_name calee_name for each unique trampoline'''
    to_osection_list = []
    from_osection_list = []
    if (trampolines_section_filter_file_name != ''):
      if (os.path.isfile(trampolines_section_filter_file_name)):
        with open(trampolines_section_filter_file_name) as fp:
            for line in fp:
              value = line.split()
              if (value[0] == "from:"):
                from_osection_list.append(value[1])
              else:
                to_osection_list.append(value[1])
        fp.close()
        print("Section to filter  :", to_osection_list)
        print("Section from filter:", from_osection_list)

    keyword1 = 'Trampolines'
    keyword2 = 'OutputSections'
    if (yamlfile.has_key_word(keyword1)):
        if (yamlfile.has_key_word(keyword2)):
          '''This means we likely have full yaml map file '''
          trampoline_map = yamlfile.get_trampolines()
          print ("Gather %d entries" % (len(trampoline_map.items())))
          for k, v in trampoline_map.items():
            if to_osection_list.count(str(v['osection'])):
              tramp_map_file.write("_tramp_entry_ %s %s\n" % (v['caller'], v['calee'] ))
        else:
          print ("No information about trampolines in map\n")
    else:
      '''This means we likely do not have full yaml map file '''
      trampoline_map = yamlfile.get_trampolines_without_sections()
      print ("Gather %d entries" % (len(trampoline_map.items())))
      specified_cnt = 0
      for k, v in trampoline_map.items():
        if to_osection_list.count(str(v['osection'])) or from_osection_list.count(str(v['tosection'])):
          tramp_map_file.write("_tramp_entry_ %s %s\n" % (v['caller'], v['calee']))
          specified_cnt += 1
      print ("For specified sections used %d entries" % (specified_cnt))
      # Create stat and transition map
      unique_tramps, reuse_tramps = yamlfile.get_trampolines_stats()
      for k in sorted(unique_tramps):
        v = unique_tramps[k]
        if k in reuse_tramps:
          tramp_stat_file.write("%s %d %d %d\n" % (k, v, reuse_tramps[k], v + reuse_tramps[k]))
        else:
          tramp_stat_file.write("%s %d 0 %d\n" % (k, v, v))
      for k, v in trampoline_map.items():
        tramp_stat_file.write("%s %s %s %s %s\n" % (v['osection'], v['tosection'], v['name'], v['caller'], v['calee']))
      tramp_stat_file.close()

def handle_xref(yamlfile):
    '''Handle --xref'''
    keyword = 'CrossReference'
    if not yamlfile.has_key_word(keyword):
        print ("No information about cross reference\n")
        return
    print("\n Section Cross References\n")
    yamlfile.get_reference()


def handle_sizes(yamlfile):
    '''Handle topic 'sizes' of --info.'''
    keyword = 'OutputSections'
    if not yamlfile.has_key_word(keyword):
        print ("No information about size\n")
        return
    print("Image component sizes")
    objs = yamlfile.get_object_sizes()
    print("     Code (inc. data)   RO Data    RW Data    ZI Data      Debug   Object Name")
    for k, v in sorted(objs.items()):
        print("{0:9d}{1:11d}{2:11d}{3:11d}{4:11d}{5:11d}   {6}".format(
                v['code']+v['data'], v['data'], v['ro_data'], v['rw_data'],
                v['zi_data'], v['debug'], os.path.basename(k)))
    obj_totals = yamlfile.get_object_totals()
    print("   ----------------------------------------------------------------------")
    print("{0:9d}{1:11d}{2:11d}{3:11d}{4:11d}{5:11d}   Object Totals".format(
            obj_totals['code']+obj_totals['data'], obj_totals['data'],
            obj_totals['ro_data'], obj_totals['rw_data'], obj_totals['zi_data'],
            obj_totals['debug']))
    generated = yamlfile.get_generated_sizes()
    print("{0:9d}{1:11d}{2:11d}{3:11d}{4:11d}{5:11d}   (incl. Generated)"
          .format(generated['code']+generated['data'], generated['data'],
                  generated['ro_data'], generated['rw_data'],
                  generated['zi_data'], generated['debug']))
    lib_objs = yamlfile.get_library_member_sizes()
    print("   ----------------------------------------------------------------------")
    print("     Code (inc. data)   RO Data    RW Data    ZI Data      Debug   Library Member Name")
    for k, v in lib_objs.items():
        print("{0:9d}{1:11d}{2:11d}{3:11d}{4:11d}{5:11d}   {6}".format(
                v['code']+v['data'], v['data'], v['ro_data'], v['rw_data'],
                v['zi_data'], v['debug'], get_library_member_name(k)))
    lib_totals = yamlfile.get_library_totals()
    print("   ----------------------------------------------------------------------")
    print("{0:9d}{1:11d}{2:11d}{3:11d}{4:11d}{5:11d}   Library Totals".format(
            lib_totals['code']+lib_totals['data'], lib_totals['data'],
            lib_totals['ro_data'], lib_totals['rw_data'], lib_totals['zi_data'],
            lib_totals['debug']))
    libs = yamlfile.get_library_sizes()
    print("   ----------------------------------------------------------------------")
    print("     Code (inc. data)   RO Data    RW Data    ZI Data      Debug   Library Name")
    for k, v in libs.items():
        print("{0:9d}{1:11d}{2:11d}{3:11d}{4:11d}{5:11d}   {6}".format(
                v['code']+v['data'], v['data'], v['ro_data'], v['rw_data'],
                v['zi_data'], v['debug'], os.path.basename(k)))
    print("   ----------------------------------------------------------------------")
    print("{0:9d}{1:11d}{2:11d}{3:11d}{4:11d}{5:11d}   Library Totals".format(
            lib_totals['code']+lib_totals['data'], lib_totals['data'],
            lib_totals['ro_data'], lib_totals['rw_data'], lib_totals['zi_data'],
            lib_totals['debug']))
    print("   ----------------------------------------------------------------------")
    grand_totals = yamlfile.get_grand_totals()
    print("=============================================================================")
    print("     Code (inc. data)   RO Data    RW Data    ZI Data      Debug")
    print("{0:9d}{1:11d}{2:11d}{3:11d}{4:11d}{5:11d}   Grand Totals".format(
            grand_totals['code']+grand_totals['data'], grand_totals['data'],
            grand_totals['ro_data'], grand_totals['rw_data'],
            grand_totals['zi_data'], grand_totals['debug']))
    print("=============================================================================")
    total_ro_size = (grand_totals['code'] + grand_totals['data']
                     + grand_totals['ro_data'])
    print("   Total RO  Size (Code + RO Data)          {0:11d} ({1:7.2f}kB)"
          .format(total_ro_size,total_ro_size/1024.0))
    total_rw_size = grand_totals['rw_data'] + grand_totals['zi_data']
    print("   Total RW  Size (RW Data + ZI Data)       {0:11d} ({1:7.2f}kB)"
          .format(total_rw_size,total_rw_size/1024.0))
    total_rom_size = (grand_totals['code'] + grand_totals['data']
                      + grand_totals['ro_data'] + grand_totals['rw_data'])
    print("   Total ROM Size (Code + RO Data + RW Data){0:11d} ({1:7.2f}kB)"
          .format(total_rom_size,total_rom_size/1024.0))
    print("=============================================================================")


def handle_summarysizes(yamlfile):
    '''Handle topic 'summarysizes' of --info.'''
    keyword = 'OutputSections'
    if not yamlfile.has_key_word(keyword):
        print ("No information about summary sizes\n")
        return
    grand_totals = yamlfile.get_grand_totals()
    print("Program Size: Code={0:d} RO-data={1:d} RW-data={2:d} ZI-data={3:d}"
          .format(grand_totals['code']+grand_totals['data'],
                  grand_totals['ro_data'], grand_totals['rw_data'],
                  grand_totals['zi_data']))


def handle_totals(yamlfile):
    '''Handle topic 'totals' of --info.'''
    keyword = 'OutputSections'
    if not yamlfile.has_key_word(keyword):
        print ("No information about totals\n")
        return
    print("Image component sizes")
    obj_totals = yamlfile.get_object_totals()
    print("     Code (inc. data)   RO Data    RW Data    ZI Data      Debug")
    print("{0:9d}{1:11d}{2:11d}{3:11d}{4:11d}{5:11d}   Object Totals".format(
            obj_totals['code']+obj_totals['data'], obj_totals['data'],
            obj_totals['ro_data'], obj_totals['rw_data'], obj_totals['zi_data'],
            obj_totals['debug']))
    generated = yamlfile.get_generated_sizes()
    print("{0:9d}{1:11d}{2:11d}{3:11d}{4:11d}{5:11d}   (incl. Generated)"
          .format(generated['code']+generated['data'], generated['data'],
                  generated['ro_data'], generated['rw_data'],
                  generated['zi_data'], generated['debug']))
    lib_totals = yamlfile.get_library_totals()
    print("{0:9d}{1:11d}{2:11d}{3:11d}{4:11d}{5:11d}   Library Totals".format(
            lib_totals['code']+lib_totals['data'], lib_totals['data'],
            lib_totals['ro_data'], lib_totals['rw_data'], lib_totals['zi_data'],
            lib_totals['debug']))
    libs = yamlfile.get_library_sizes()
    grand_totals = yamlfile.get_grand_totals()
    print("=============================================================================")
    print("Grand Totals Loaded Sections:")
    print("     Code (inc. data)   RO Data    RW Data    ZI Data")
    print("{0:9d}{1:11d}{2:11d}{3:11d}{4:11d}".format(
            grand_totals['code']+grand_totals['data'], grand_totals['data'],
            grand_totals['ro_data'], grand_totals['rw_data'],
            grand_totals['zi_data']))
    print("Grand Totals Sections Not Loaded:")
    print("     Code (inc. data)   RO Data    RW Data    ZI Data    Debug")
    print("{0:9d}{1:11d}{2:11d}{3:11d}{4:11d}{5:11d}".format(
            grand_totals['code_no_load']+grand_totals['data_no_load'], grand_totals['data_no_load'],
            grand_totals['ro_data_no_load'], grand_totals['rw_data_no_load'],
            grand_totals['zi_data_no_load'], grand_totals['debug']))
    print("=============================================================================")
    print("Totals sections loaded:")
    total_ro_size = (grand_totals['code'] + grand_totals['data']
                     + grand_totals['ro_data'])
    print("   Total RO  Size (Code + RO Data)          {0:11d} ({1:7.2f}kB)"
          .format(total_ro_size,total_ro_size/1024.0))
    total_rw_size = grand_totals['rw_data'] + grand_totals['zi_data']
    print("   Total RW  Size (RW Data + ZI Data)       {0:11d} ({1:7.2f}kB)"
          .format(total_rw_size,total_rw_size/1024.0))
    total_rom_size = (grand_totals['code'] + grand_totals['data']
                      + grand_totals['ro_data'] + grand_totals['rw_data'])
    print("   Total ROM Size (Code + RO Data + RW Data){0:11d} ({1:7.2f}kB)"
          .format(total_rom_size,total_rom_size/1024.0))

    print("Totals sections not loaded:")
    ro_no_load = (grand_totals['code_no_load'] + grand_totals['data_no_load']
                     + grand_totals['ro_data_no_load'])
    print("   NoLoad RO  Size (Code + RO Data sections not loaded)          {0:11d} ({1:7.2f}kB)"
          .format(ro_no_load,ro_no_load/1024.0))
    rw_no_load = grand_totals['rw_data_no_load'] + grand_totals['zi_data_no_load']
    print("   NoLoad RW  Size (RW Data + ZI Data sections not loaded)       {0:11d} ({1:7.2f}kB)"
          .format(rw_no_load,rw_no_load/1024.0))
    rom_no_load = (grand_totals['code_no_load'] + grand_totals['data_no_load']
                      + grand_totals['ro_data_no_load'] + grand_totals['rw_data_no_load'])
    print("   NoLoad ROM Size (Code + RO Data + RW Data sections not loaded){0:11d} ({1:7.2f}kB)"
          .format(rom_no_load,rom_no_load/1024.0))
    not_loaded = grand_totals['no_load']
    print("   NoLoad total (total size sections not loaded inc. debug sections){0:8d} ({1:7.2f}kB)"
          .format(not_loaded,not_loaded/1024.0))
    print("=============================================================================")


def handle_architecture(yamlfile):
    '''Handle topic 'architecture' of --info.'''
    print("Architecture: " + yamlfile.get_architecture())
    print("Emulation: " + yamlfile.get_emulation())
    print("AddressSize: " + yamlfile.get_address_size())
    print("ABIPageSize: " + hex(yamlfile.get_abi_page_size()))

def get_symbol_type(sym, yamlfile):
    type = sym['Type']
    if 'STT_SECT' in type:
        return 'Section'
    if 'STT_FUNC' in type:
        if yamlfile.get_architecture() == 'aarch64':
            return  'AT64 Code'
        if sym['Region']:
            return sym['Region']
        return 'Code'
    if 'STT_OBJECT' in type:
        return 'Number'
    return sym['Region']

def get_symbols(yamlfile):
    symbols = []
    region = ''
    for osec in yamlfile.yamlfile['OutputSections']:
        if 'Contents' not in osec or not osec['Contents']:
            continue
        for isec in osec['Contents']:
            if 'Symbols' not in isec:
                continue
            for sym in isec['Symbols']:
                name = sym['Symbol']
                if name.startswith('$d.'):
                    region = 'Data'
                    continue
                if yamlfile.get_architecture() == 'aarch64':
                    if name.startswith('$x.'):
                        region = 'AT64 Code'
                        continue
                else:
                    if name.startswith('$t.'):
                        region = 'Thumb Code'
                        continue
                    if name.startswith('$a.'):
                        region = 'ARM Code'
                        continue
                sym['Origin'] = os.path.basename(isec['Origin']) + '(' + isec['Name'] + ')'
                sym['Region'] = region
                symbols.append(sym)
    return symbols

def handle_symbols(yamlfile):
    '''Handle Local and Global symbols'''
    title = '    Symbol Name                              Value     Ov Type        Size  Object(Section)'
    addr_field = '8'
    if yamlfile.get_address_size() == '64bit':
        addr_field = '16'
    title_fmt = '    {:<40s} {:<' + addr_field + 's}   {:<10s} {:5s} {:s}'
    title = title_fmt.format('Symbol Name', 'Value', 'Ov Type', 'Size', 'Object(Section)')
    fmt = '    {:40s} {:#0' + addr_field + 'x}   {:10s}{:#5d}   {:s}'

    print ('==============================================================================')
    print ('Image Symbol Table')
    print ('    Local Symbols')
    print (title)
    symbols = get_symbols(yamlfile)
    for sym in symbols:
        sym_type = get_symbol_type(sym, yamlfile)
        if sym['Scope'] and sym_type != 'Section' : # local
            print(fmt.format(sym['Symbol'], sym['Value'], sym_type, sym['Size'], sym['Origin']))
    print ('    Global Symbols')
    print (title)
    for sym in symbols:
        sym_type = get_symbol_type(sym, yamlfile)
        if sym['Scope'] == 0 and sym_type != 'Section': #global
            print(fmt.format(sym['Symbol'], sym['Value'], sym_type, sym['Size'], sym['Origin']))
    print ('==============================================================================')

def handle_unused(yamlfile):
    '''Handle topic 'unused' of --info.'''
    keyword = 'DiscardedSections'
    if not yamlfile.has_key_word(keyword):
        print ('No information about unused')
        return
    print("Removing Unused input sections from the image.")
    unused = yamlfile.get_unused()
    total_size = 0
    for k,v in unused.items():
        total_size += v['size']
        print("  Removing {0}({1}), ({2:d} bytes).".format(
                os.path.basename(v['origin']), k, v['size']))
    print("{0:d} unused section(s) (total {1:d} bytes) removed from the image."
          .format(len(unused), total_size))


def handle_unusedsymbols(yamlfile):
    '''Handle topic 'unusedsymbols' of --info.'''
    keyword = 'DiscardedSections'
    if not yamlfile.has_key_word(keyword):
        print ('No information about unusedsymbols')
        return
    print("Removing Unused input sections from the image.")
    unused_symbols = yamlfile.get_unused_symbols()
    for sym in unused_symbols:
        print("       Removing symbol {0}.".format(sym))
    print("{0:d} unused symbol(s) removed from the image.".format(
            len(unused_symbols)))

def handle_unusedobjects(yamlfile):
    '''Handle topic 'unusedobjects' of --info.'''
    keyword = 'InputInfo'
    if not yamlfile.has_key_word(keyword):
        print ('No information about unusedobjects')
        return
    print("Unused objects from the inputs.")
    yamlfile.get_unused_objects()

def handle_info(yaml_file, info):
    '''Handle --info=topic[,topic,...].'''
    usupported_topic = ""
    info_list = info.split(',')
    for topic in info_list:
        if topic == 'sizes':
            handle_sizes(yaml_file)
        elif topic == 'summarysizes':
            handle_summarysizes(yaml_file)
        elif topic == 'totals':
            handle_totals(yaml_file)
        elif topic == 'architecture':
            handle_architecture(yaml_file)
        elif topic == 'unused':
            handle_unused(yaml_file)
        elif topic == 'unusedsymbols':
            handle_unusedsymbols(yaml_file)
        elif topic == 'unusedobjects':
            handle_unusedobjects(yaml_file)
        else:
            usupported_topic += topic
            print('Unsupported topic: ' + topic)


def main():
    parser = argparse.ArgumentParser(description='YAML parser', usage=
                                     '%(prog)s [-h] [--info=topic[,topic,...]] [--map] [--symbols] [--list=file] [--xref] YAML_file',
                                     formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('yaml_file', help='The input YAML file.')
    parser.add_argument('--info', help='''List misc. information about image.
 Available topics: (separate multiple topics with comma)
  architecture  List architecture.
  sizes         List code and data sizes for objects in image.
  summarysizes  List code and data sizes of the image.
  totals        List total sizes of all objects in image.
  unused        List sections eliminated from the image.
  unusedsymbols List symbols eliminated from the image.
  unusedobjects List objects eliminated from the inputs.''')
    parser.add_argument('--map', help='Display memory map of image.',
                        action='store_true')
    parser.add_argument('--symbols', help='Display symbols in image.',
                        action='store_true')
    parser.add_argument('--list', help='Redirect output to a file.')
    parser.add_argument('--xref', help=
                        'List all cross-references between input sections.',
                        action='store_true')
    ''' This option takes a list of sections for which trampoline logging is desired.
        For instance if file contains the following entry: .text.pagable
        Only trampolines allocated in this section will be logged in the detailed list
        (See the --trampolines flag)'''
    parser.add_argument('--trampolines_section_filter_file', help=
                        'List of output sections to use as filter')
    ''' Create caller->calee map for which a trampoline was seen in the map file.
        This map can be used for trampoline suppression during compilation.'''
    parser.add_argument('--trampolines', help=
                        'List detail information for each trampoline and create tramp map in provided file')
    args = parser.parse_args()
    with open(args.yaml_file, 'r') as f:
        yamlfile = YAMLFile(f)

    if args.list:
        output_file = open(args.list, 'w')
        sys.stdout = output_file

    if args.symbols:
        handle_symbols(yamlfile)

    if args.info:
        handle_info(yamlfile, args.info)

    if args.map:
        handle_map(yamlfile)

    if args.xref:
        handle_xref(yamlfile)

    trampolines_section_filter_file_name = ''
    if args.trampolines_section_filter_file:
      trampolines_section_filter_file_name = args.trampolines_section_filter_file

    if args.trampolines:
      tramp_map_file = open(args.trampolines, 'w')
      tramp_stat_file = open(args.trampolines + '_stat', 'w')
      handle_trampolines(yamlfile, tramp_map_file, tramp_stat_file, trampolines_section_filter_file_name)
      tramp_map_file.close()
    if args.list:
        output_file.close()

if __name__ == '__main__':
    main()
