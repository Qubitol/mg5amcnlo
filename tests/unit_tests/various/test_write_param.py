################################################################################
#
# Copyright (c) 2011 The MadGraph5_aMC@NLO Development team and Contributors
#
# This file is a part of the MadGraph5_aMC@NLO project, an application which 
# automatically generates Feynman diagrams and matrix elements for arbitrary
# high-energy processes in the Standard Model and beyond.
#
# It is subject to the MadGraph5_aMC@NLO license which should accompany this 
# distribution.
#
# For more information, visit madgraph.phys.ucl.ac.be and amcatnlo.web.cern.ch
#
################################################################################
from __future__ import division
from __future__ import absolute_import
import random
import six
StringIO = six
import os

import tests.unit_tests as unittest

import madgraph.core.base_objects as base_objects

import models.import_ufo as import_ufo
import models.write_param_card as writter
from six.moves import range
import madgraph.various.misc as misc 


_file_path = os.path.split(os.path.dirname(os.path.realpath(__file__)))[0]

class TestParamWritting(unittest.TestCase):
    """Test that MG5 succesfully write a param_card."""
    
    def setUp(self):
        """ prepare a model and the ParamCardWriter"""

        # load the SM
        self.model = import_ufo.import_model('sm')
        # initialize the main object 
        self.writter = writter.ParamCardWriter(self.model)
        self.content = StringIO.StringIO()
        self.writter.define_output_file(self.content)
        self.content.truncate(0) # remove the header
        self.content.seek(0)
        
    def test_create_param_dict(self):
        """Check that the dictionary is valid."""
        
        out = self.writter.param_dict
        for key, obj in out.items():
            self.assertIsInstance(key, str)
            self.assertIsInstance(obj, base_objects.ModelVariable)
            self.assertEqual(key, obj.name)
            
    def test_define_not_dep_param(self):
        """Check that we found all mass-width which are not external."""
        
        for part, obj in self.writter.dep_mass:    
            self.assertIsInstance(part, base_objects.Particle)
            self.assertIsInstance(obj, base_objects.ModelVariable)
            self.assertNotIsInstance(obj, base_objects.ParamCardVariable)
            self.assertEqual(part['mass'], obj.name)
        
        for part, obj in self.writter.dep_width:    
            self.assertIsInstance(part, base_objects.Particle)
            self.assertIsInstance(obj, base_objects.ModelVariable)
            self.assertNotIsInstance(obj, base_objects.ParamCardVariable)
            self.assertEqual(part['width'], obj.name)

        
        # Check the presence of mass-width for A/G/W
        checked_mass = [21,22,24]
        for part, obj in self.writter.dep_mass:
            if part['pdg_code'] in checked_mass:
                checked_mass.remove(part['pdg_code'])
        self.assertEqual(checked_mass,[])
        
        checked_width = [21,22]
        for part, obj in self.writter.dep_width:
            if part['pdg_code'] in checked_width:
                checked_width.remove(part['pdg_code'])
        self.assertEqual(checked_width, [])
        
        # Check the duplicate mass-width
        self.assertEqual(self.writter.duplicate_mass, [])
        self.assertEqual(self.writter.duplicate_width, [])
        
        
        
    def test_order_param(self):
        """Check that we can correctly order two parameter."""
        
        p1 = base_objects.ParamCardVariable('p1', 1, 'first', [1])
        p2 = base_objects.ParamCardVariable('p2', 1, 'first', [2])
        p3 = base_objects.ParamCardVariable('p3', 1, 'first', [3])
        p4 = base_objects.ParamCardVariable('p4', 1, 'first', [3, 1])
        p5 = base_objects.ParamCardVariable('p5', 1, 'first', [3, 2])
        p6 = base_objects.ParamCardVariable('p6', 1, 'second', [1])
        p7 = base_objects.ParamCardVariable('p7', 1, 'second', [1, 3])
        p8 = base_objects.ParamCardVariable('p8', 1, 'second', [2])
        p9 = base_objects.ParamCardVariable('p9', 1, 'DECAY', [2])

        result = [p1, p2, p3, p4, p5, p6, p7, p8, p9]
        start =result[:]
        for i in range(20):
            random.shuffle(start)
            start.sort(key=misc.cmp_to_key(self.writter.order_param))
            self.assertEqual(start, result, 
               '%s != %s' % ([p.name for p in start], [p.name for p in result]))
        
    def test_write_block(self):
        """Check that the writting of a block works"""
        
        self.writter.write_block('DECAY')
        self.assertNotIn('Block', self.content.getvalue())
        self.writter.write_block('mass')
        self.assertIn('Block mass', self.content.getvalue())
        
    def test_write_param(self):
        """Check that the writting of a parameter works"""
        
        param = base_objects.ParamCardVariable('p8', 0.54 + 0j, 'first', [2,4])
        
        # check for standard block
        self.writter.write_param(param, 'mass')
        result = "    2   4 5.400000e-01 # p8 \n"
        self.assertEqual(result, self.content.getvalue())
        self.content.truncate(0)
        self.content.seek(0)
        
        # check for block decay
        self.writter.write_param(param, 'DECAY')
        result = "DECAY   2   4 5.400000e-01 # p8 \n"
        self.assertEqual(result, self.content.getvalue())
        self.content.truncate(0)
        self.content.seek(0)
                
        # check that fail on complex number
        wrongparam =  base_objects.ParamCardVariable('p8', 0.54 + 2j, 'first', [2,4])
        self.assertRaises(writter.ParamCardWriterError,self.writter.write_param,
                           wrongparam, 'masss')
        
        # check that this is fine on integer
        param = base_objects.ParamCardVariable('p8', 2, 'first', [2,4])
        self.writter.write_param(param, 'mass')
        result = "    2   4 2.000000e+00 # p8 \n"
        self.assertEqual(result, self.content.getvalue())
        self.content.truncate(0)               
        self.content.seek(0)
                
    def test_write_qnumber(self):
        """ check if we can writte qnumber """
        
        particleList = base_objects.ParticleList()
        particle = base_objects.Particle()
        particleList.append(particle)
        
        self.model.set('particles', particleList)
        
        particle.set('pdg_code', 100)
        particle.set('color', 1)
        particle.set('spin', 1)
        particle.set('charge', 0.0)        
        self.writter.write_qnumber()
        
        text = self.content.getvalue()
        self.assertIn('Block QNUMBERS 100', text)
        self.assertIn('1 0', text)
        self.assertIn('2 1', text)
        self.assertIn('3 1', text)
        self.assertIn('4 1', text)
        
        # a second particle
        particle.set('pdg_code', 40)
        particle.set('color', 3)
        particle.set('spin', 3)
        particle.set('charge', 1/3) 
        particle.set('self_antipart', True) 
        self.content.truncate(0)
        self.writter.write_qnumber()
         
        text = self.content.getvalue()
        self.assertIn('Block QNUMBERS 40', text)
        self.assertIn('1 1', text)
        self.assertIn('2 3', text)
        self.assertIn('3 3', text)
        self.assertIn('4 0', text)
 
        
class TestParamWrittingWithRestrict(unittest.TestCase):
    """Test that MG5 succesfully write a param_card."""
    
    def setUp(self):
        """ prepare a model and the ParamCardWriter"""

        # load the SM with restriction
        self.model = import_ufo.import_model('sm-full')
        self.model = import_ufo.RestrictModel(self.model)
        self.restrict_file = os.path.join(_file_path, os.path.pardir,
                                     'input_files', 'restrict_sm.dat')
        self.model.restrict_model(self.restrict_file)
        
        # initialize the main object 
        self.writter = writter.ParamCardWriter(self.model)
        self.content = StringIO.StringIO()
        self.writter.define_output_file(self.content)
        self.content.truncate(0) # remove the header
        self.content.seek(0) # need in py3 to fully remove header
        
    def test_define_not_dep_param(self):
        """Check that we found all mass-width which are not external."""
        
        self.writter.define_not_dep_param()
        
        for part, obj in self.writter.dep_mass:    
            self.assertIsInstance(part, base_objects.Particle)
            self.assertIsInstance(obj, base_objects.ModelVariable)
            self.assertNotIsInstance(obj, base_objects.ParamCardVariable)
            self.assertEqual(part['mass'], obj.name)
        
        for part, obj in self.writter.dep_width:    
            self.assertIsInstance(part, base_objects.Particle)
            self.assertIsInstance(obj, base_objects.ModelVariable)
            self.assertNotIsInstance(obj, base_objects.ParamCardVariable)
            self.assertEqual(part['width'], obj.name)

        
        # Check the presence of mass-width for A/G/W
        checked_mass = [21,22,24,23,25]
        for part, obj in self.writter.dep_mass:
            if part['pdg_code'] in checked_mass:
                checked_mass.remove(part['pdg_code'])
        self.assertEqual(len(checked_mass), 2)
        self.assertIn(checked_mass[0], [23,25])
        self.assertIn(checked_mass[1], [23,25])
        
        # Check that 23/25 are in a duplicate state
        self.assertEqual(len(self.writter.duplicate_mass),1)
        for part, obj in self.writter.duplicate_mass:
            if part['pdg_code'] in checked_mass:
                checked_mass.remove(part['pdg_code'])
        self.assertEqual(len(checked_mass), 1)       
        
        if checked_mass[0] == 23:
            self.assertEqual(self.writter.param_dict['mdl_MH'].expr, '1*mdl_MZ')
        if checked_mass[0] == 25:
            self.assertEqual(self.writter.param_dict['mdl_MZ'].expr, '1*mdl_MH')
        
        
    def test_full_write(self):
        """ test that we can write a file """ 
        
        self.writter.write_card()
        
        goal = """
###################################
## INFORMATION FOR MASS
###################################
Block mass 
    6 1.720000e+02 # MT 
   15 1.777000e+00 # MTA 
   23 9.118800e+01 # set of param :1*MZ, 1*MH 
## Dependent parameters, given by model restrictions.
## Those values should be edited following the 
## analytical expression. MG5 ignores those values 
## but they are important for interfacing the output of MG5
## to external program such as Pythia.
  1 0.000000e+00 # d : 0.0 
  2 0.000000e+00 # u : 0.0 
  3 0.000000e+00 # s : 0.0 
  4 0.000000e+00 # c : 0.0 
  5 0.000000e+00 # b : 0.0 
  11 0.000000e+00 # e- : 0.0 
  12 0.000000e+00 # ve : 0.0 
  13 0.000000e+00 # mu- : 0.0 
  14 0.000000e+00 # vm : 0.0 
  16 0.000000e+00 # vt : 0.0 
  21 0.000000e+00 # g : 0.0 
  22 0.000000e+00 # a : 0.0 
  24 8.041900e+01 # w+ : cmath.sqrt(MZ__exp__2/2. + cmath.sqrt(MZ__exp__4/4. - (aEW*cmath.pi*MZ__exp__2)/(Gf*sqrt__2))) 
  25 9.118800e+01 # h : MZ 

###################################
## INFORMATION FOR SMINPUTS
###################################
Block sminputs 
    1 1.325070e+02 # aEWM1 
    2 1.166390e-05 # Gf 
    3 1.180000e-01 # aS (Note: this Parameter is not used if you use a PDF set) 

###################################
## INFORMATION FOR YUKAWA
###################################
Block yukawa 
    6 1.645000e+02 # ymt 
   15 1.777000e+00 # ymtau 

###################################
## INFORMATION FOR DECAY
###################################
DECAY  23 2.441404e+00 # WZ 
DECAY  24 3.000000e+00 # WW 
DECAY  25 2.441404e+00 # WH 
## Dependent parameters, given by model restrictions.
## Those values should be edited following the 
## analytical expression. MG5 ignores those values 
## but they are important for interfacing the output of MG5
## to external program such as Pythia.
DECAY  1 0.000000e+00 # d : 0.0 
DECAY  2 0.000000e+00 # u : 0.0 
DECAY  3 0.000000e+00 # s : 0.0 
DECAY  4 0.000000e+00 # c : 0.0 
DECAY  5 0.000000e+00 # b : 0.0 
DECAY  6 0.000000e+00 # t : 0.0 
DECAY  11 0.000000e+00 # e- : 0.0 
DECAY  12 0.000000e+00 # ve : 0.0 
DECAY  13 0.000000e+00 # mu- : 0.0 
DECAY  14 0.000000e+00 # vm : 0.0 
DECAY  15 0.000000e+00 # ta- : 0.0 
DECAY  16 0.000000e+00 # vt : 0.0 
DECAY  21 0.000000e+00 # g : 0.0 
DECAY  22 0.000000e+00 # a : 0.0 
""".split('\n')

        text = self.content.getvalue()
        #text = text.encode()
        #misc.sprint(type(text))
        self.assertEqual(text.split('\n'), goal)
    
