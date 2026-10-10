import pandas as pd
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler

df=pd.read_csv('dataset/data.csv')

X=df.drop('target', axis=1)
y=df['target']

X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

scaler = StandardScaler()
X_train_scaled = scaler.fit_transform(X_train)
X_test_scaled = scaler.transform(X_test)

os.makedirs('preprocessed_data', exist_ok=True)
pd.DataFrame(X_train_scaled, columns=X.columns).to_csv('preprocessed_data/X_train_scaled.csv', index=False)
pd.DataFrame(X_test_scaled, columns=X.columns).to_csv('preprocessed_data/X_test_scaled.csv', index=False)
y_train.to_csv('preprocessed_data/y_train.csv', index=False)
y_test.to_csv('preprocessed_data/y_test.csv', index=False)