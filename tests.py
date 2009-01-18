import unittest
import geoquad

class RatioTestCase(unittest.TestCase):
	def assertAlmostEqual(self, a, b, precision=0.99):
		assert abs(a - b) < min(a, b) * (1 - precision)
	assertAlmostEquals = assertAlmostEqual

class Geoquadtester(RatioTestCase):

	def test_create_then_parse(self):
		'''
		Test interleave -> deinterleave.
		'''
		lat, lng = (10, 20)
		g = geoquad.create(lat, lng)
		lat_, lng_ = geoquad.parse(g)
		self.assertAlmostEqual(lat, lat_)
		self.assertAlmostEqual(lng, lng_)
	
if __name__ == '__main__':
	unittest.main()
