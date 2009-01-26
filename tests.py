import unittest
import geoquad

class RatioTestCase(unittest.TestCase):
	def assertAlmostEqual(self, a, b, precision=0.995):
		assert abs(a - b) < min(a, b) * (1 - precision), '%1.4f !~ %1.4f [precision = %1.3f]' % (a, b, precision)
	assertAlmostEquals = assertAlmostEqual

class GeoquadTestCase(RatioTestCase):

	def test_create_then_parse(self):
		lat, lng = (10.01, 20.01)
		g = geoquad.create(lat, lng)
		lat_, lng_ = geoquad.parse(g)
		assert geoquad.contains(g, lat_, lng_)

	def test_northof(self):
		lat, lng = (10, 20)
		g1 = geoquad.create(lat, lng)
		g2 = geoquad.northof(g1)
		lat_, lng_ = geoquad.parse(g2)
		assert geoquad.contains(g2, lat_, lng_)

	def test_southof(self):
		lat, lng = (10, 20)
		g1 = geoquad.create(lat, lng)
		g2 = geoquad.southof(g1)
		lat_, lng_ = geoquad.parse(g2)
		assert geoquad.contains(g2, lat_, lng_)

	def test_eastof(self):
		lat, lng = (10, 20)
		g1 = geoquad.create(lat, lng)
		g2 = geoquad.eastof(g1)
		lat_, lng_ = geoquad.parse(g2)
		assert geoquad.contains(g2, lat_, lng_)

	def test_westof(self):
		lat, lng = (10, 20)
		g1 = geoquad.create(lat, lng)
		g2 = geoquad.westof(g1)
		lat_, lng_ = geoquad.parse(g2)
		assert geoquad.contains(g2, lat_, lng)

	def test_nearby(self):
		lat, lng = (10, 20)
		g = geoquad.create(lat, lng)
		assert len(geoquad.nearby(g, 0.1)) == 14
		assert len(geoquad.nearby(g, 0.5)) == 309

if __name__ == '__main__':
	unittest.main()
