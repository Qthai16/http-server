-- example HTTP POST script which demonstrates setting the
-- HTTP method, body, and adding a header

wrk.method = "POST"
-- wrk.body   = "foo=bar&baz=quux"
wrk.headers["Content-Type"] = "application/octet-stream"
-- wrk.headers["Content-Disposition"] = "attachment; filename=\"th.jpeg\""
file = io.open("test-data/hochiminh_city.jpg", "rb")
wrk.body   = file:read("*a")
