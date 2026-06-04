module error {

  func make_error(message) {
    var error = new {};
    error.message = message;
    return error;
  }

}
