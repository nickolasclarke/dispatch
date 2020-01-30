def DropColumnsIfPresent(df, columns_to_drop):
  """Drop columns from Pandas DataFrame regardless of whether they are there.

  Dropping columns that don't exist from a Pandas DataFrame raises an exception.
  This function avoids that.
  """

  columns_to_drop = [x for x in columns_to_drop if x in df.index]
  return df.drop(columns=columns_to_drop)
